"""SWZC Frame Generation crash fix: build, self-test, install, remove, status, package.

Usage (Python 3, Windows; install / uninstall / status need `pefile` and use GAME_BIN below):
  python fgfix.py build       build.bat -> build/version.dll
  python fgfix.py selftest    loads the DLL here (it only forwards outside the game) and runs FGFix_SelfTestA
  python fgfix.py install     copies version.dll into the game's Binaries\\Win64 (refuses over a version.dll that is
                              not this fix, e.g. another mod's: remove that first)
  python fgfix.py uninstall   removes it (only if it is this fix) and its log
  python fgfix.py status      installed or not, and SWZCFrameGenFix.log
  python fgfix.py package     dist/SWZCFrameGenFix_1.0.zip laid out like the game's top folder:
                              SWZeroCompany/Binaries/Win64/version.dll + README.txt
"""
import ctypes, os, shutil, subprocess, sys, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
DLL = os.path.join(HERE, "build", "version.dll")
README = os.path.join(HERE, "README.txt")
GAME_BIN = r"A:\SteamLibrary\steamapps\common\Star Wars Zero Company\SWZeroCompany\Binaries\Win64"
LOG = os.path.join(GAME_BIN, "SWZCFrameGenFix.log")
VERSION = "1.0"


def exports(path):
    import pefile
    try:
        pe = pefile.PE(data=open(path, "rb").read(), fast_load=True)
        pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]])
        return {e.name for e in pe.DIRECTORY_ENTRY_EXPORT.symbols if e.name}
    except Exception:
        return set()


def is_fix(path):
    return b"FGFix_SelfTestA" in exports(path) and b"GM_SelfTest" not in exports(path)


def build():
    r = subprocess.run(["cmd", "/c", os.path.join(HERE, "build.bat")], capture_output=True, text=True)
    print("\n".join(l for l in (r.stdout + r.stderr).splitlines() if "vswhere" not in l and "operable" not in l))
    return r.returncode


def selftest():
    h = ctypes.WinDLL(DLL)
    buf = ctypes.create_string_buffer(512)
    ok = h.FGFix_SelfTestA(buf, 512)
    print(buf.value.decode())
    return 0 if ok else 1


def install():
    dst = os.path.join(GAME_BIN, "version.dll")
    if os.path.exists(dst) and not is_fix(dst):
        raise SystemExit("a version.dll that is not this fix is in " + GAME_BIN + " (another mod?): remove it first")
    shutil.copy2(DLL, dst)
    print("installed", dst)


def uninstall():
    dst = os.path.join(GAME_BIN, "version.dll")
    if os.path.exists(dst):
        if not is_fix(dst):
            raise SystemExit("the version.dll in " + GAME_BIN + " is not this fix: not touching it")
        os.remove(dst)
        print("removed", dst)
    if os.path.exists(LOG):
        os.remove(LOG)
        print("removed", LOG)


def status():
    dst = os.path.join(GAME_BIN, "version.dll")
    print("installed:", os.path.exists(dst) and is_fix(dst))
    print("log:", open(LOG, encoding="utf-8", errors="replace").read().strip() if os.path.exists(LOG) else "(none yet)")


def package():
    out_dir = os.path.join(HERE, "dist")
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, "SWZCFrameGenFix_%s.zip" % VERSION)
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        z.write(DLL, "SWZeroCompany/Binaries/Win64/version.dll")
        z.write(README, "SWZCFrameGenFix_README.txt")
    with zipfile.ZipFile(out) as z:
        assert z.testzip() is None
        print(out, [i.filename for i in z.infolist()])


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "status"
    fn = {"build": build, "selftest": selftest, "install": install, "uninstall": uninstall, "status": status,
          "package": package}.get(cmd)
    if not fn:
        print(__doc__)
        sys.exit(2)
    r = fn()
    sys.exit(r if isinstance(r, int) else 0)
