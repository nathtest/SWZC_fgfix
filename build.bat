@echo off
rem Builds build\version.dll (the Frame Generation crash fix) with the x64 C++ tools of Visual Studio 2022
rem (any edition, or the Build Tools, with the "Desktop development with C++" workload).
setlocal
cd /d "%~dp0"
where ml64 >nul 2>nul && goto build
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (echo Visual Studio not found & exit /b 1)
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS=%%i"
if not defined VS (echo Visual Studio with the x64 C++ tools not found & exit /b 1)
set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
:build
if not exist build mkdir build
ml64 /nologo /c /Fobuild\stubs.obj stubs.asm || exit /b 1
cl /nologo /O2 /W3 /MT /GS /c /Fobuild\fgfix.obj fgfix.c || exit /b 1
link /nologo /DLL /OUT:build\version.dll /DEF:fgfix.def build\fgfix.obj build\stubs.obj kernel32.lib || exit /b 1
echo built %~dp0build\version.dll
