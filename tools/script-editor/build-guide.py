#!/usr/bin/env python3
"""Render docs/script-editor.md as an offline guide. Requires Markdown."""
from pathlib import Path
import re
import markdown

root = Path(__file__).resolve().parents[2]
source = (root/'docs/script-editor.md').read_text(encoding='utf-8')
source = source.replace('](../examples/', '](examples/')
for name in ('worlded-build.md',):
    source = source.replace(']('+name+')', '](docs/'+name+')')
body = markdown.markdown(source, extensions=['fenced_code', 'tables', 'toc'])
page = '''<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Script Editor Renaissance — Guide</title>
<style>
:root{color-scheme:light dark}body{font:17px/1.65 system-ui,sans-serif;max-width:960px;margin:3rem auto;padding:0 1.5rem}
h1,h2{line-height:1.2}h2{margin-top:2.5rem}a{color:light-dark(#12599b,#86bfff)}
code{font-size:.92em}pre{overflow:auto;padding:1.2rem;background:light-dark(#f0f3f6,#202830);border-radius:6px}
table{border-collapse:collapse;width:100%;font-size:.94em}td,th{padding:.65rem;border:1px solid #8885;text-align:left}
@media print{body{font-size:11pt;margin:0;max-width:none}pre{white-space:pre-wrap}h2{break-after:avoid}}
</style><main>'''+body+'</main></html>\n'
(root/'script-editor.html').write_text(page, encoding='utf-8')

# Keep the main offline manual self-contained. Its navigation and other sections
# are hand-authored; these two sections share the maintained Markdown guides.
manual_path = root/'worlded-tutorial.html'
manual = manual_path.read_text(encoding='utf-8')
def section(identifier, title, content):
    content = content.split('\n', 1)[1]
    content = re.sub(r'^(#{2,}) ', r'#\1 ', content, flags=re.M)
    html = markdown.markdown(content, extensions=['fenced_code', 'tables'])
    html = html.replace('<table>', '<div class="table-wrap"><table>').replace('</table>', '</table></div>')
    return f'<section id="{identifier}"><h2>{title}</h2>\n{html}</section>'

scripts = section('scripts', 'Scripts and conversations', source)
manual = re.sub(r'<section id="scripts">.*?</section>', lambda _:scripts, manual, flags=re.S)
mod_source = (root/'docs/worlded-modding.md').read_text(encoding='utf-8')
modding = section('modding', 'Create a mod for Arcanum Renaissance', mod_source)
if '<section id="modding">' in manual:
    manual = re.sub(r'<section id="modding">.*?</section>', lambda _:modding, manual, flags=re.S)
else:
    manual = manual.replace('<section id="housing">', modding+'\n<section id="housing">')
    manual = manual.replace('<li><a href="#housing">', '<li><a href="#modding">Create a mod for Renaissance</a></li><li><a href="#housing">')
number = 0
def heading(match):
    global number
    number += 1
    title = re.sub(r'^\d+\. ', '', match[2])
    return match[1]+str(number)+'. '+title+'</h2>'
manual = re.sub(r'(<section id="[^"]+"><h2>)(.*?)</h2>', heading, manual)
manual_path.write_text(manual, encoding='utf-8', newline='\r\n')
