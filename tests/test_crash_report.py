"""Exercise real unhandled Windows faults in disposable child processes."""
from pathlib import Path
import os
import shutil
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="renaissance-crash-") as temporary:
    work = Path(temporary) / "Chemin accentué"
    work.mkdir()
    exe = work / "check-crash.exe"
    source = ROOT / "tools/check-crash-report.c"
    include = ROOT / "source/src"
    if os.name != "nt":
        subprocess.run(["cc", "-Wall", "-Wextra", "-Werror", f"-I{include}", str(source), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
        print("PASS: non-Windows startup remains unchanged (Windows runtime not tested)")
        raise SystemExit(0)

    compiled = Path(temporary) / "check-crash.exe"
    compile_args = ["cl", "/nologo", "/W4", "/O2", "/D_CRT_SECURE_NO_WARNINGS", f"/I{include}", str(source), f"/Fe{compiled}", f"/Fo{Path(temporary) / 'check.obj'}"]
    if shutil.which("cl"):
        subprocess.run(compile_args, cwd=ROOT, check=True)
    else:
        vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
        vs = subprocess.check_output([str(vswhere), "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
        assert vs, "MSVC C++ tools are required"
        script = Path(temporary) / "compile.cmd"
        script.write_text(f'@echo off\ncall "{vs}\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n' + subprocess.list2cmdline(compile_args) + "\n", encoding="mbcs")
        subprocess.run(["cmd", "/c", str(script)], cwd=ROOT, check=True)
    shutil.move(compiled, exe)

    reports = work / "ArcanumModern/CrashReports"
    manifest = work / "ArcanumModern/manifest.json"
    manifest.parent.mkdir(exist_ok=True)
    manifest.write_text('{"version":"test","source":{"revision":"fixture"}}')
    subprocess.run([str(exe)], cwd=work, check=True)
    assert not list(reports.iterdir()), "Normal exit must not create a crash report"

    def crash(mode, directory, dump_expected=True, env=None):
        before = set(directory.glob("*.txt"))
        child = subprocess.run([str(exe), mode], cwd=work, env=env, timeout=20)
        assert child.returncode & 0xFFFFFFFF == 0xC0000005, hex(child.returncode & 0xFFFFFFFF)
        created = set(directory.glob("*.txt")) - before
        assert len(created) == 1, created
        report = created.pop()
        text = report.read_text()
        assert "Exception: 0xC0000005" in text and "Arcanum Renaissance" in text, text
        assert report.with_suffix(".manifest.json").read_bytes() == manifest.read_bytes()
        dump = report.with_suffix(".dmp")
        if dump_expected:
            assert "0 = success): 0" in text, text
            data = dump.read_bytes()
            signature, _, count, table = struct.unpack_from("<4sIII", data)
            assert signature == b"MDMP"
            streams = {kind: (size, rva) for kind, size, rva in (struct.unpack_from("<III", data, table + 12 * i) for i in range(count))}
            assert {3, 4, 6}.issubset(streams), streams  # threads, modules, exception
            _, rva = streams[6]
            assert struct.unpack_from("<I", data, rva + 8)[0] == 0xC0000005
            assert struct.unpack_from("<Q", data, rva + 24)[0] != 0  # fault instruction
            context_size, context_rva = struct.unpack_from("<II", data, rva + 160)
            assert context_size and context_rva + context_size <= len(data)
        else:
            assert not dump.exists() and "0 = success): 127" in text, text

    crash("fault", reports)
    crash("fault", reports)  # preserve previous reports
    crash("no-dbghelp", reports, False)
    shutil.move(str(manifest.parent), str(work / "previous-reports"))
    manifest.parent.write_text("Block installation-local report directory")
    # A file at ArcanumModern forces the writable user-profile fallback.
    fallback_root = work / "profile"
    fallback_root.mkdir()
    fallback = fallback_root / "ArcanumRenaissance/CrashReports"
    env = dict(os.environ, LOCALAPPDATA=str(fallback_root))
    child = subprocess.run([str(exe), "fault"], cwd=work, env=env, timeout=20)
    assert child.returncode & 0xFFFFFFFF == 0xC0000005
    assert len(list(fallback.glob("*.dmp"))) == 1 and len(list(fallback.glob("*.txt"))) == 1
    print("PASS: Windows access violation, valid exception/context/module dump, normal exit, unique reports, missing DbgHelp and profile fallback")
