#!/usr/bin/env python3
"""Bundle the offline editor into one HTML file; no toolchain or network needed."""
import argparse
import base64
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--mes-directory', type=Path, help='Optional local folder containing stat.mes, skill.mes and spell.mes; text is embedded only in the ignored dist artifact.')
args = parser.parse_args()
source = root / "tools/save-editor"
html = (source / "index.html").read_text(encoding='utf-8')
css = (source / "style.css").read_text(encoding='utf-8')
for font in ("Heading", "Book"):
    path = root / f"game/modern-ui/{font}.ttf"
    if path.exists():
        uri = "data:font/ttf;base64," + base64.b64encode(path.read_bytes()).decode()
        css = css.replace(f"../../game/modern-ui/{font}.ttf", uri)
html = html.replace('<link rel="stylesheet" href="style.css">', f"<style>{css}</style>")
if args.mes_directory:
    messages = {}
    for name, ids in [('stat', set(range(28))), ('skill', set(range(20))),
                      ('spell', set(range(80)) | set(range(500, 516)))]:
        data = (args.mes_directory / f'{name}.mes').read_bytes()
        try:
            text = data.decode('utf-8')
        except UnicodeDecodeError:
            text = data.decode('cp1252')
        messages[name] = [[int(key), value] for key, value in re.findall(r'\{\s*(-?\d+)\s*\}\s*\{([^}]*)\}', text) if int(key) in ids]
        if not ids.issubset({key for key, _ in messages[name]}):
            raise SystemExit(f'Missing labels in {name}.mes')
    payload = json.dumps(messages, ensure_ascii=True).replace('<', '\\u003c')
    html = html.replace('</body>', f'<script>globalThis.ArcanumLocalMessages = {payload};</script>\n</body>')
for script in ("schema.js", "save-format.js", "app.js"):
    html = html.replace(f'  <script src="{script}" defer></script>\n', "")
    code = (source / script).read_text(encoding='utf-8').replace("</script", "<\\/script")
    html = html.replace("</body>", f"<script>\n{code}\n</script>\n</body>")
output = root / "dist/arcanum-save-editor.html"
output.parent.mkdir(exist_ok=True)
output.write_text(html, encoding='utf-8')
print(output)
