#!/usr/bin/env python3
"""Record only our edits to files in the pinned upstream engine."""
from pathlib import Path
import difflib
import os
import subprocess

project = Path(__file__).resolve().parents[1]
upstream = Path(os.environ.get("ARCANUM_ENGINE_SOURCE", "/tmp/arcanum-ce-source"))
revision = "a7ff41b300ef712f0e7d088183a3d08957110cdd"
files = subprocess.check_output(
    ["git", "-C", str(upstream), "ls-tree", "-r", "--name-only", revision], text=True
).splitlines()
patch = []
for relative in files:
    destination = project / "source" / relative
    if not destination.is_file():
        raise SystemExit(f"Missing engine file: {relative}")
    before = subprocess.check_output(["git", "-C", str(upstream), "show", f"{revision}:{relative}"])
    after = destination.read_bytes()
    if before.replace(b"\r\n", b"\n") == after.replace(b"\r\n", b"\n"):
        continue
    patch.append(f"diff --git a/{relative} b/{relative}\n")
    patch.extend(difflib.unified_diff(
        before.decode("utf-8").splitlines(keepends=True),
        after.decode("utf-8").splitlines(keepends=True),
        fromfile=f"a/{relative}", tofile=f"b/{relative}"))
directory = project / "patches"
directory.mkdir(exist_ok=True)
(directory / "engine.patch").write_text("".join(patch))
print(f"Recorded {sum(line.startswith('diff --git ') for line in patch)} modified engine files.")
