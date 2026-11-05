"""Exercise the real SDL launcher renderer without a desktop session."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('renderer', type=Path)
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='renaissance-ui-test-') as directory:
    state = Path(directory)/'state.txt'
    for language, status, play, auto in (
        ('fr', 'Téléchargement de v2.0.2 · 64 %', 'Jouer sans mise à jour', 'Lancer le jeu automatiquement'),
        ('en', 'Downloading v2.0.2 · 64%', 'Play without updating', 'Launch the game automatically'),
    ):
        state.write_text('64 true true true\n'+status+'\nVersion: 2.0.1\nRELEASE NOTES · v2.0.2\n'+play+'\n'+auto+'\n'
                         + ('Portraits personnalisés, compétences et antécédents. A long release note wraps across the panel.\n'*25), encoding='utf-8')
        subprocess.run([str(args.renderer.resolve()), str(state), str(ROOT/'tools/launcher/assets/background.png'),
                        str(ROOT/'game/modern-ui/Book.ttf'), '--check-render'], check=True,
                       env=dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_VIDEO_DRIVER='dummy', SDL_RENDER_DRIVER='software'))
        print('PASS:', language, 'launcher rendering')
