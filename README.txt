SWZC Frame Generation Crash Fix 1.0 (temporary community fix)
===============================================================

WHAT IT FIXES
Random crashes of STAR WARS Zero Company when loading a save, entering or leaving a map, or returning to
the main menu, while NVIDIA DLSS Frame Generation is on. The crash reporter shows an access violation in
the engine's garbage collection, for example:
  - UE::GC::TBatchDispatcher<...>::FlushQueuedReferences  (reading a small address such as 0x8, 0x21)
  - UMaterialInstance::FinishDestroy -> FMallocBinned2::Free  (reading address 0x3)
  - UEngine::LoadMap -> TrimMemory -> CollectGarbage -> IncrementalPurgeGarbage -> ...
The crash is random because it depends on what happens to lie in memory at the damaged spot.

THE CAUSE
The game contains both NVIDIA's DLSS Frame Generation (Streamline) and AMD's FSR3 frame interpolation.
Every frame, the FSR3 plugin updates a status field in the game window's "present" object, without checking
that the object is its own. With DLSS Frame Generation on, that object belongs to NVIDIA Streamline and is
much smaller, so FSR3's writes (3 bytes) land in an unrelated piece of game memory 160 bytes further on.
Sooner or later that memory is an object list the engine's garbage collector walks, and the game crashes at
the next big cleanup, typically while loading a save.

WHAT THIS FIX DOES
At game start it patches that one FSR3 function in memory so it only touches FSR3's own objects. FSR3 frame
generation (AMD) works exactly as before, and DLSS Frame Generation keeps its memory intact. No game file is
modified: the patch only exists in memory while the game runs, and the cost is a few comparisons per frame.
The fix finds the function by its code, not by a fixed address, so it works on any game version that still
contains the faulty FSR3 code. If the code is not found exactly once, it changes nothing and says so in its log.

WHO NEEDS IT
Players with DLSS Frame Generation enabled (Graphics settings, Frame Generation 2x/3x/4x with DLSS).
DLSS Frame Generation only exists on NVIDIA GeForce RTX 40 and RTX 50 series cards, so only those
players can be affected. It is harmless for everyone else.

REQUIREMENTS
Windows, the Steam version of the game. Tested on the build of 15 October 2026 (199049). If a game update
removes the faulty code, the log says "NOT APPLIED" and the fix can be deleted.

INSTALL
Copy version.dll into the folder that holds SWZeroCompany.exe:
  ...\steamapps\common\Star Wars Zero Company\SWZeroCompany\Binaries\Win64\
(or extract the zip into ...\steamapps\common\Star Wars Zero Company\).
Start the game once, then open SWZCFrameGenFix.log in that same folder: it must say "APPLIED".

UNINSTALL
Delete version.dll and SWZCFrameGenFix.log from that folder.

COMPATIBILITY
Conflicts with any other mod that also places a version.dll next to SWZeroCompany.exe.
Works with content mods.

WITHOUT THIS FIX
Turning DLSS Frame Generation off avoids the crash too. Restart the game after changing the setting.

Not affiliated with Bit Reactor, Electronic Arts or Lucasfilm. Remove the fix once the game is patched.
