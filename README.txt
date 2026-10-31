SWZC Frame Generation Crash Fix 1.0 (temporary community fix)
===============================================================

WHAT IT FIXES
Random crashes of STAR WARS Zero Company with NVIDIA DLSS Frame Generation on, mostly when loading a save
(often a Den save) and sometimes when quitting.

WHO NEEDS IT
Players with an NVIDIA GeForce RTX 40 or RTX 50 series card and DLSS Frame Generation on (Graphics settings).
Other cards cannot use DLSS Frame Generation, so they are not affected. The fix is harmless for everyone else.

WHY IT CRASHES
The game includes two frame generation technologies, NVIDIA's and AMD's. Even when AMD's is off, a small
part of it keeps running, and with NVIDIA's on, it writes a few bytes into memory that belongs to something
else. Most of the time nothing happens; sometimes the damaged memory makes the game crash later, usually
when loading a save. That is why the crash seems random.

WHAT THE FIX DOES
It stops that bit of AMD code from writing into memory that is not its own. Nothing else changes, and no
game file is modified: the fix only acts while the game runs. If a game update changes that code, the fix
does nothing and says so in its log.

INSTALL
Extract the zip into the game's folder:
  ...\steamapps\common\Star Wars Zero Company\
(this puts version.dll into SWZeroCompany\Binaries\Win64\, next to SWZeroCompany.exe).
Start the game once, then open SWZCFrameGenFix.log in that same Win64 folder: it should say "APPLIED".

UNINSTALL
Delete version.dll and SWZCFrameGenFix.log from SWZeroCompany\Binaries\Win64\.

COMPATIBILITY
Works with content mods. Conflicts with any other mod that also puts a version.dll in that folder.
Tested with the Steam version of the game, patch 1.2 (15 October 2026).

WITHOUT THIS FIX
Turning DLSS Frame Generation off also avoids the crash. Restart the game after changing the setting.

Source code and technical details: see the mod page.
Not affiliated with Bit Reactor, Electronic Arts or Lucasfilm. Remove the fix once the game is patched.
