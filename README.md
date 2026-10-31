# SWZC Frame Generation Crash Fix

Source of the community fix for random crashes of STAR WARS Zero Company with NVIDIA DLSS Frame Generation on
(RTX 40 / 50 series): crashes in the engine's garbage collection while loading a save, changing map or quitting.
Player instructions are in [README.txt](README.txt), which ships in the release zip.

## The bug

The game contains both NVIDIA Streamline (DLSS Frame Generation) and AMD's FSR3 frame interpolation. Every frame, FSR3's
window callback (`FFXFrameInterpolation::OnSlateWindowRendered`, its render-thread lambda) calls
`FFXFrameInterpolationCustomPresent::SetCustomPresentStatus` on the window's custom present, checking only that it is not
null. With DLSS Frame Generation on, that present is Streamline's `FStreamlineDLSSGCustomPresent`, a 16-byte object, so
the status writes (2 at +0xA0, 1 at +0xA9, 1 at +0xAA) land in an unrelated 16-byte heap block 160 bytes further on.
Whether the game crashes depends on what the allocator puts in that block: when it is an object array or a pointer the
garbage collector follows, the next full GC crashes.

## The fix

`version.dll`, loaded by the game from its own folder, forwards every `version.dll` export to the System32 copy and, at
load, patches `SetCustomPresentStatus` in memory (no game file is changed):

- The function is found by its 103 bytes of code, exactly or with any struct field offsets. Exactly one match is
  required, otherwise nothing is changed and the log says so.
- Its first instruction becomes a jump to a trampoline that returns at once unless the object is one of FSR's own
  presents (its first virtual functions sit in the same code module as the function), then runs the displaced
  instructions and jumps back. FSR's own presents behave exactly as before.

Files: `fgfix_core.c` (search, patch, self-test), `fgfix.c` (the DLL: forwarding, log), `stubs.asm` and `fgfix.def`
(the forwarded exports), `build.bat`, `fgfix.py` (build, self-test, install, package helpers).

## Build

Needs Visual Studio 2022 or its Build Tools with the "Desktop development with C++" workload.

```
build.bat
```

The output is `build\version.dll`. To check it (Python 3, no game needed; the DLL only forwards outside the game):

```
python fgfix.py selftest
```

The self-test runs the patch on a copy of the original function: the original corrupts a foreign object, the patched
one leaves it untouched and gives FSR's own object the same writes as before for every status.

Built from this source, the DLL's code (`.text`) is byte-identical to the release's; only the build timestamp differs.

## Install

Copy `build\version.dll` next to `SWZeroCompany.exe`
(`...\steamapps\common\Star Wars Zero Company\SWZeroCompany\Binaries\Win64\`), start the game, and check that
`SWZCFrameGenFix.log` in the same folder says `APPLIED`. Delete both files to uninstall.

## License

Public domain ([The Unlicense](LICENSE)): use, change and redistribute it freely.

Not affiliated with Bit Reactor, Electronic Arts or Lucasfilm.
