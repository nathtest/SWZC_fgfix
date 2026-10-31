/* SWZC Frame Generation crash fix: a version.dll for SWZeroCompany\Binaries\Win64 that applies fgfix_core.c's patch at
   load and writes one line to SWZCFrameGenFix.log next to the game's exe. Every version.dll export jumps to the real System32
   copy (stubs.asm, fgfix.def), so the game sees no difference. Removing the file removes the fix. */

#include "fgfix_core.c"
#include <wchar.h>

#define VERSION_FUNCS(X) X(GetFileVersionInfoA) X(GetFileVersionInfoByHandle) X(GetFileVersionInfoExA) \
  X(GetFileVersionInfoExW) X(GetFileVersionInfoSizeA) X(GetFileVersionInfoSizeExA) X(GetFileVersionInfoSizeExW) \
  X(GetFileVersionInfoSizeW) X(GetFileVersionInfoW) X(VerFindFileA) X(VerFindFileW) X(VerInstallFileA) \
  X(VerInstallFileW) X(VerQueryValueA) X(VerQueryValueW)
#define DECL_PTR(n) void* gm_p_##n = 0;
VERSION_FUNCS(DECL_PTR)

static BOOL ResolveVersion(void)
{
    wchar_t path[MAX_PATH];
    UINT n = GetSystemDirectoryW(path, MAX_PATH);
    if (!n || n > MAX_PATH - 20) return FALSE;
    wcscat_s(path, MAX_PATH, L"\\version.dll");
    HMODULE m = LoadLibraryW(path);
    if (!m) return FALSE;
#define RESOLVE(n) gm_p_##n = (void*)GetProcAddress(m, #n); if (!gm_p_##n) return FALSE;
    VERSION_FUNCS(RESOLVE)
    return TRUE;
}

static void WriteLog(const char* text)
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);   /* next to the game's exe, wherever the DLL was loaded from */
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash) return;
    wcscpy_s(slash + 1, MAX_PATH - (slash + 1 - path), L"SWZCFrameGenFix.log");
    HANDLE h = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    char line[1024];
    int len = _snprintf_s(line, sizeof line, _TRUNCATE, "%04d-%02d-%02d %02d:%02d:%02d SWZC Frame Generation crash fix 1.0: %s\r\n",
                          t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, text);
    DWORD w;
    if (len > 0) WriteFile(h, line, (DWORD)len, &w, NULL);
    CloseHandle(h);
}

/* for the developer's test (python ctypes): runs the patch on a copy of the function, never on the game */
__declspec(dllexport) int FGFix_SelfTestA(char* out, int n)
{
    return FGFix_SelfTest(out, (size_t)n);
}

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(self);
        if (!ResolveVersion()) return FALSE;
        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(NULL, exe, MAX_PATH);
        const wchar_t* base = wcsrchr(exe, L'\\');
        base = base ? base + 1 : exe;
        if (_wcsicmp(base, L"SWZeroCompany.exe") != 0) return TRUE;   /* loaded by another program: forward only */
        char msg[768];
        FGFix_Apply(msg, sizeof msg);
        WriteLog(msg);
    }
    return TRUE;
}
