#!/usr/bin/env python3
"""Merge existing CMake/Ninja build commands for IntelliJ's C/C++ plugin.

This only reads build metadata. It never builds, configures or runs the game.
"""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def build_commands(build, fallback_ninja=None):
    database = build / "compile_commands.json"
    if database.is_file():
        return json.loads(database.read_text(encoding="utf-8"))
    if not (build / "build.ninja").is_file():
        raise ValueError(
            f"{build}: configure this build with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "
            "or use the Ninja generator first"
        )
    cache = build / "CMakeCache.txt"
    cached_ninja = None
    if cache.is_file():
        match = re.search(r"^CMAKE_MAKE_PROGRAM:[^=]+=(.+)$", cache.read_text(), re.M)
        if match and Path(match[1].strip()).is_file():
            cached_ninja = match[1].strip()
    ninja = cached_ninja or shutil.which("ninja") or fallback_ninja
    if not ninja:
        raise ValueError(f"{build}: Ninja is needed to read this build's commands")
    rules = subprocess.run(
        [ninja, "-C", str(build), "-t", "rules"],
        check=True, capture_output=True, text=True, timeout=30,
    ).stdout.splitlines()
    rules = [rule for rule in rules if re.match(r"(?:C|CXX|OBJC|OBJCXX)_COMPILER", rule)]
    if not rules:
        raise ValueError(f"{build}: no native compiler rules found")
    result = subprocess.run(
        [ninja, "-C", str(build), "-t", "compdb", "-x", *rules],
        check=True, capture_output=True, text=True, timeout=30,
    )
    return json.loads(result.stdout)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("builds", nargs="*", type=Path,
                        help="Configured build directories (default: build and build/worlded)")
    parser.add_argument("--output", type=Path, default=ROOT / "build/ide/compile_commands.json")
    args = parser.parse_args()
    builds = args.builds or [ROOT / "build", ROOT / "build/worlded"]
    sdk = Path(os.environ.get("ARCANUM_BUILD_SDK", "/tmp/arcanum-ce-sdk"))
    sdk_ninja = sdk / "usr/bin/ninja"
    fallback_ninja = str(sdk_ninja) if sdk_ninja.is_file() else None
    commands = {}
    for build in builds:
        build = build.resolve()
        entries = build_commands(build, fallback_ninja)
        if not isinstance(entries, list):
            raise ValueError(f"{build}: expected a compilation database array")
        if not entries:
            raise ValueError(f"{build}: empty compilation database")
        for entry in entries:
            if not isinstance(entry, dict) or not all(
                isinstance(entry.get(key), str) and entry[key]
                for key in ("directory", "file")
            ):
                raise ValueError(f"{build}: each command needs a directory and source file")
            # Paths in the combined database must keep their original build context.
            directory = Path(entry["directory"])
            if not directory.is_absolute():
                directory = build / directory
            entry["directory"] = str(directory.resolve())
            source = Path(entry["file"])
            source = (directory / source).resolve() if not source.is_absolute() else source.resolve()
            entry["file"] = str(source)
            if not isinstance(entry.get("command"), str) and not isinstance(entry.get("arguments"), list):
                raise ValueError(f"{build}: missing compiler command for {source}")
            # Prefer the first build's context when a harness also compiles engine code.
            commands.setdefault(str(source), entry)
        print(f"{build.relative_to(ROOT) if build.is_relative_to(ROOT) else build}: {len(entries)} commands")
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    # Preserve the previous complete index if input validation or writing fails.
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=output.parent,
                                         prefix=".compile_commands-", delete=False) as stream:
            temporary = Path(stream.name)
            json.dump(list(commands.values()), stream, indent=2, ensure_ascii=False)
            stream.write("\n")
        os.replace(temporary, output)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    print(f"{output}: {len(commands)} distinct source files")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        raise SystemExit(f"Native index: {error}") from error
