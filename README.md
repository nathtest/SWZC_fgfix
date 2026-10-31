# SWZC Frame Generation Crash Fix

Source of the community fix for random crashes of STAR WARS Zero Company with NVIDIA DLSS Frame Generation on
(RTX 40 / 50 series): crashes, mostly in the engine's garbage collection, while loading a save or quitting.
Download: [Nexus Mods](https://www.nexusmods.com/starwarszerocompany/mods/404). Player instructions are in
[README.txt](README.txt), which ships in the release zip.

## The bug

The game contains both NVIDIA Streamline (DLSS Frame Generation) and AMD's FSR3 frame interpolation. Every frame, FSR3's
window callback (`FFXFrameInterpolation::OnSlateWindowRendered`, its render-thread lambda) calls
`FFXFrameInterpolationCustomPresent::SetCustomPresentStatus` on the window's custom present, checking only that it is not
null. It does so even with FSR3 frame interpolation off: every early exit of the callback, `r.FidelityFX.FI.Enabled` 0
included, still runs that render command (on the test machine, NVIDIA with DLSS Frame Generation on, the cvar stayed 0
for the whole session). With DLSS Frame Generation on, that present is Streamline's `FStreamlineDLSSGCustomPresent`, a 16-byte object, so
the status writes (2 at +0xA0, 1 at +0xA9, 1 at +0xAA) land in an unrelated 16-byte heap block 160 bytes further on.
Whether the game crashes depends on what the allocator puts in that block: when it is an object array or a pointer the
garbage collector follows, the next full GC crashes.

## For the game's developers

Found on patch 1.2 (build 199049). Repro: an RTX 40 / 50 card, DLSS with Frame Generation 2x; with a page heap on
16-byte blocks the write is caught at the main menu on every launch.

- The object: Streamline's `FStreamlineDLSSGCustomPresent` (16 bytes, allocated in `FStreamlineCoreModule::StartupModule`),
  set as the viewport's custom present.
- The read: `FFXFrameInterpolation::OnSlateWindowRendered` casts `GetCustomPresent()` to
  `FFXFrameInterpolationCustomPresent` and reads its byte at +0xAC, outside Streamline's object.
- The write: the render command `UpdateWindowBackBufferCommand` then calls
  `FFXFrameInterpolationCustomPresent::SetCustomPresentStatus` on that object (statuses 2 and 3: +0xA0, +0xA9, +0xAA)
  on every path, `r.FidelityFX.FI.Enabled` 0 included.
- Evidence: all 4 full crash dumps and 16 of 20 crash minidumps show those three bytes in the block 0xA0 after
  Streamline's present (of the other 4, two are empty, one lacks that memory, one shows the present undamaged), and the
  crashing pointer is often that block's first qword.
- To fix it (any one): let the FSR3 plugin touch the viewport's custom present only when it is a present FSR3 created
  (compare against its own before the cast, in the game-thread check and in the render command); or skip the callback
  entirely while FSR3 frame interpolation is off; or don't register FSR3's frame interpolation callbacks while DLSS
  Frame Generation owns the viewport.

## The fix

`version.dll`, loaded by the game from its own folder, forwards every `version.dll` export to the System32 copy and, at
load, patches `SetCustomPresentStatus` in memory (no game file is changed):

- The function is found by its 103 bytes of code, exactly or with any struct field offsets. Exactly one match is
  required, otherwise nothing is changed and the log says so.
- Its first instruction becomes a jump to a trampoline that returns at once unless the object is one of FSR's own
  presents (its first virtual functions sit in the same code module as the function), then runs the displaced
  instructions and jumps back. FSR's own presents behave exactly as before; any other present, such as Streamline's,
  is left untouched, so the callback that runs with FSR3 off becomes harmless.

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

The release DLL was built with MSVC 14.38: with that toolset, a build from this source has byte-identical code.

## Install

Copy `build\version.dll` next to `SWZeroCompany.exe`
(`...\steamapps\common\Star Wars Zero Company\SWZeroCompany\Binaries\Win64\`), start the game, and check that
`SWZCFrameGenFix.log` in the same folder says `APPLIED`. Delete both files to uninstall.

## License

Public domain ([The Unlicense](LICENSE)): use, change and redistribute it freely.

Not affiliated with Bit Reactor, Electronic Arts or Lucasfilm.
