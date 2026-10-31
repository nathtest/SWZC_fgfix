/* fgfix core: the code patch for the save-load crash with DLSS Frame Generation.

   The bug: FSR3 frame interpolation's window callback (FFXFrameInterpolation::OnSlateWindowRendered, its render-thread
   lambda) calls FFXFrameInterpolationCustomPresent::SetCustomPresentStatus on whatever custom present the window has,
   every frame. With DLSS Frame Generation on, that present is NVIDIA Streamline's FStreamlineDLSSGCustomPresent, a
   16-byte object, and the status writes (2 at +0xA0, 1 at +0xA9, 1 at +0xAA) land in an unrelated heap block 160 bytes
   further on. When that block is an object array the garbage collector walks, the next full GC (the save load) crashes.

   The patch, version-independent (the bug spans game versions):
   - SetCustomPresentStatus is found by its code: the 103 bytes of game build 199049 (exact), or else the same
     instructions with any struct field offsets (FGFIX_MASK). Exactly one match is required; otherwise nothing changes.
   - Its first instruction becomes a jump to a trampoline that asks FGFix_IsOwn(object) and returns at once unless the
     object is one of FSR's own presents, then runs the two displaced instructions and jumps back.
   - FGFix_IsOwn needs no addresses either: FSR's present class keeps its first virtual functions in the same code module
     as SetCustomPresentStatus (build 199049: slots 0-2 are 4 to 5 KB from it), while a foreign present's are elsewhere
     (Streamline's: 20 MB and more away). At least 2 of the first 3 slots within 1 MB means "FSR's own".
   FSR's own presents behave exactly as before; any other object is left alone. Applied at DLL load, before any game code
   runs, so no thread can be inside the function while its first bytes change. */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* SetCustomPresentStatus as shipped in build 199049, position independent: only short jumps inside itself */
static const uint8_t FGFIX_ORIGINAL[103] = {
    0x44, 0x0f, 0xb6, 0xc2, 0x84, 0xd2, 0x74, 0x32, 0x41, 0x83, 0xe8, 0x01, 0x74, 0x14, 0x41, 0x83,
    0xe8, 0x01, 0x74, 0x44, 0x41, 0x83, 0xf8, 0x01, 0x75, 0x4c, 0xc6, 0x81, 0xaa, 0x00, 0x00, 0x00,
    0x01, 0xc3, 0x83, 0xb9, 0xa4, 0x00, 0x00, 0x00, 0x01, 0xc6, 0x81, 0xb0, 0x00, 0x00, 0x00, 0x01,
    0x74, 0xe8, 0xc6, 0x81, 0xaa, 0x00, 0x00, 0x00, 0x00, 0xc3, 0x83, 0xb9, 0xa4, 0x00, 0x00, 0x00,
    0x01, 0xc6, 0x81, 0xaf, 0x00, 0x00, 0x00, 0x01, 0x74, 0x0e, 0x88, 0x91, 0xa0, 0x00, 0x00, 0x00,
    0xc6, 0x81, 0xa9, 0x00, 0x00, 0x00, 0x00, 0xc3, 0xc6, 0x81, 0xa0, 0x00, 0x00, 0x00, 0x02, 0xc6,
    0x81, 0xa9, 0x00, 0x00, 0x00, 0x01, 0xc3
};
/* 0 = a struct field offset (the disp32 of the 10 memory operands), free to differ in another build */
static const uint8_t FGFIX_MASK[103] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
    1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0,
    1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1,
    1, 0, 0, 0, 0, 1, 1
};
#define FGFIX_DISPLACED 6       /* movzx r8d, dl (4) + test dl, dl (2): no relative addressing */
#define FGFIX_NEAR 0x100000     /* "same code module": within 1 MB of SetCustomPresentStatus */

static uint8_t* volatile g_fgTarget;   /* the patched function, the reference point of FGFix_IsOwn */

/* 1 when `obj` is one of FSR's own presents (its first virtual functions sit next to SetCustomPresentStatus) */
static int FGFix_IsOwn(void* obj)
{
    __try {
        const uintptr_t* vt = *(const uintptr_t* const*)obj;
        int near_ = 0;
        for (int i = 0; i < 3; i++) {
            intptr_t d = (intptr_t)vt[i] - (intptr_t)g_fgTarget;
            if (d > -FGFIX_NEAR && d < FGFIX_NEAR) near_++;
        }
        return near_ >= 2;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;   /* unreadable: certainly not FSR's, leave it alone */
    }
}

/* executable memory within +-2 GB of `near`, so a 5-byte jmp rel32 reaches it */
static uint8_t* FGFix_AllocNear(uint8_t* near_)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    uintptr_t gran = si.dwAllocationGranularity, t = (uintptr_t)near_;
    for (uintptr_t d = gran; d < 0x7FE00000u; d += gran) {
        if (t > d) {
            void* p = VirtualAlloc((void*)((t - d) & ~(gran - 1)), 4096, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
            if (p) return (uint8_t*)p;
        }
        void* p = VirtualAlloc((void*)((t + d) & ~(gran - 1)), 4096, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (p) return (uint8_t*)p;
    }
    return NULL;
}

/* patch SetCustomPresentStatus at `target` so that it only runs for FSR's own presents */
static int FGFix_PatchAt(uint8_t* target, char* msg, size_t n)
{
    if (memcmp(target, FGFIX_ORIGINAL, FGFIX_DISPLACED) != 0) {
        _snprintf_s(msg, n, _TRUNCATE, "the function's first bytes are not the expected ones (already patched?)");
        return 0;
    }
    uint8_t* tramp = FGFix_AllocNear(target);
    if (!tramp) { _snprintf_s(msg, n, _TRUNCATE, "no memory near the game code for the trampoline"); return 0; }
    intptr_t rel = (intptr_t)tramp - (intptr_t)(target + 5);
    if (rel < INT32_MIN || rel > INT32_MAX) { _snprintf_s(msg, n, _TRUNCATE, "trampoline out of jump range"); return 0; }
    g_fgTarget = target;
    uint64_t check = (uint64_t)(uintptr_t)&FGFix_IsOwn, back = (uint64_t)(uintptr_t)(target + FGFIX_DISPLACED);
    uint8_t* p = tramp;
    /* entry: rsp = 16k+8, rcx = the object, dl = the status */
    *p++ = 0x51;                                                   /* push rcx */
    *p++ = 0x52;                                                   /* push rdx */
    *p++ = 0x48; *p++ = 0x83; *p++ = 0xEC; *p++ = 0x28;            /* sub rsp, 0x28   (shadow space; rsp 16-aligned) */
    *p++ = 0x48; *p++ = 0xB8; memcpy(p, &check, 8); p += 8;        /* mov rax, FGFix_IsOwn */
    *p++ = 0xFF; *p++ = 0xD0;                                      /* call rax        (rcx = the object) */
    *p++ = 0x48; *p++ = 0x83; *p++ = 0xC4; *p++ = 0x28;            /* add rsp, 0x28 */
    *p++ = 0x5A;                                                   /* pop rdx */
    *p++ = 0x59;                                                   /* pop rcx */
    *p++ = 0x85; *p++ = 0xC0;                                      /* test eax, eax */
    *p++ = 0x74; *p++ = 0x14;                                      /* jz -> ret       not FSR's own present */
    memcpy(p, target, FGFIX_DISPLACED); p += FGFIX_DISPLACED;     /* movzx r8d, dl ; test dl, dl  (flags for the je) */
    *p++ = 0xFF; *p++ = 0x25; *p++ = 0; *p++ = 0; *p++ = 0; *p++ = 0; /* jmp [rip+0] -> the original's 7th byte */
    memcpy(p, &back, 8); p += 8;
    *p++ = 0xC3;                                                   /* ret */
    DWORD old;
    VirtualProtect(tramp, 4096, PAGE_EXECUTE_READ, &old);
    FlushInstructionCache(GetCurrentProcess(), tramp, 4096);
    uint8_t patch[FGFIX_DISPLACED] = { 0xE9, 0, 0, 0, 0, 0x90 };  /* jmp rel32 ; nop */
    int32_t rel32 = (int32_t)rel;
    memcpy(patch + 1, &rel32, 4);
    if (!VirtualProtect(target, FGFIX_DISPLACED, PAGE_EXECUTE_READWRITE, &old)) {
        _snprintf_s(msg, n, _TRUNCATE, "could not unprotect the game code (error %lu)", GetLastError());
        return 0;
    }
    memcpy(target, patch, FGFIX_DISPLACED);
    DWORD dummy;
    VirtualProtect(target, FGFIX_DISPLACED, old, &dummy);
    FlushInstructionCache(GetCurrentProcess(), target, FGFIX_DISPLACED);
    _snprintf_s(msg, n, _TRUNCATE, "trampoline %p", tramp);
    return 1;
}

static int FGFix_Matches(const uint8_t* p, int masked)
{
    for (int i = 0; i < (int)sizeof FGFIX_ORIGINAL; i++)
        if (p[i] != FGFIX_ORIGINAL[i] && (!masked || FGFIX_MASK[i])) return 0;
    return 1;
}

/* SetCustomPresentStatus in the exe's code: exactly one exact match, or else exactly one match up to field offsets */
static uint8_t* FGFix_Find(uint8_t* exe, int* how, int* count)
{
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(exe + ((IMAGE_DOS_HEADER*)exe)->e_lfanew);
    IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (int masked = 0; masked < 2; masked++) {
        uint8_t* found = NULL;
        int c = 0;
        for (int s = 0; s < nt->FileHeader.NumberOfSections; s++) {
            if (!(sec[s].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
            uint8_t* a = exe + sec[s].VirtualAddress;
            size_t size = sec[s].Misc.VirtualSize;
            if (size < sizeof FGFIX_ORIGINAL) continue;
            for (size_t i = 0; i + sizeof FGFIX_ORIGINAL <= size; i++) {
                if (a[i] != 0x44 || a[i + 1] != 0x0f || a[i + 2] != 0xb6) continue;   /* movzx r8d, dl */
                if (FGFix_Matches(a + i, masked)) { found = a + i; c++; }
            }
        }
        if (c) { *how = masked; *count = c; return c == 1 ? found : NULL; }
    }
    *how = -1;
    *count = 0;
    return NULL;
}

/* the game: find the function, then patch it */
static int FGFix_Apply(char* msg, size_t n)
{
    uint8_t* exe = (uint8_t*)GetModuleHandleW(NULL);
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(exe + ((IMAGE_DOS_HEADER*)exe)->e_lfanew);
    int how, count;
    uint8_t* target = FGFix_Find(exe, &how, &count);
    if (!target) {
        if (count > 1)
            _snprintf_s(msg, n, _TRUNCATE, "NOT APPLIED: the FSR3 function was found %d times (game exe %08lx), so it is not "
                        "patched; nothing was changed.", count, nt->FileHeader.TimeDateStamp);
        else
            _snprintf_s(msg, n, _TRUNCATE, "NOT APPLIED: the FSR3 function this fix patches is not in this game version (exe %08lx); "
                        "nothing was changed. If the game no longer crashes with DLSS Frame Generation, it may be fixed: remove "
                        "version.dll.", nt->FileHeader.TimeDateStamp);
        return 0;
    }
    char why[256];
    if (!FGFix_PatchAt(target, why, sizeof why)) {
        _snprintf_s(msg, n, _TRUNCATE, "NOT APPLIED: %s", why);
        return 0;
    }
    _snprintf_s(msg, n, _TRUNCATE, "APPLIED: FSR3 frame interpolation now only updates its own present objects (game exe %08lx, "
                "function at +0x%llx, %s match; %s)", nt->FileHeader.TimeDateStamp, (unsigned long long)(target - exe),
                how ? "field-offset" : "exact", why);
    return 1;
}

/* self test on a copy of the original function: FSR's present keeps its exact behaviour, any other object is untouched */
static int FGFix_SelfTest(char* msg, size_t n)
{
    typedef void (*PFN)(void*, uint8_t);
    uint8_t* fn = (uint8_t*)VirtualAlloc(NULL, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!fn) { _snprintf_s(msg, n, _TRUNCATE, "selftest: no memory"); return 0; }
    memcpy(fn, FGFIX_ORIGINAL, sizeof FGFIX_ORIGINAL);
    /* FSR's own vtable: slots next to the function (as in the game, 4-5 KB away); a foreign one: slots far away */
    static uintptr_t ownVt[4], otherVt[4];
    for (int i = 0; i < 4; i++) { ownVt[i] = (uintptr_t)(fn + 0x1000 + 0x100 * i); otherVt[i] = (uintptr_t)&GetTickCount; }
    static uint8_t own[0x100], other[0x100], before[0x100];
    int fails = 0;
#define FGCHECK(c) do { if (!(c)) fails++; } while (0)
    /* the original code on a foreign object: this is the corruption */
    memset(other, 0, sizeof other); *(void**)other = otherVt;
    ((PFN)fn)(other, 2);
    FGCHECK(other[0xA0] == 2 && other[0xA9] == 1);
    char why[256];
    if (!FGFix_PatchAt(fn, why, sizeof why)) { _snprintf_s(msg, n, _TRUNCATE, "selftest: patch failed: %s", why); return 0; }
    /* FSR's own object: same results as the original, status by status */
    memset(own, 0, sizeof own); *(void**)own = ownVt;
    FGCHECK(FGFix_IsOwn(own) == 1);
    ((PFN)fn)(own, 2); FGCHECK(own[0xA0] == 2 && own[0xA9] == 1);
    ((PFN)fn)(own, 3); FGCHECK(own[0xAA] == 1);
    *(int*)(own + 0xA4) = 0; ((PFN)fn)(own, 0); FGCHECK(own[0xAF] == 1 && own[0xA0] == 0 && own[0xA9] == 0);
    *(int*)(own + 0xA4) = 1; own[0xAA] = 0; ((PFN)fn)(own, 1); FGCHECK(own[0xB0] == 1 && own[0xAA] == 1);
    *(int*)(own + 0xA4) = 0; ((PFN)fn)(own, 1); FGCHECK(own[0xAA] == 0);
    memset(own + 0xA0, 0, 0x10); *(int*)(own + 0xA4) = 1; ((PFN)fn)(own, 0); FGCHECK(own[0xAF] == 1 && own[0xA0] == 2 && own[0xA9] == 1);
    /* any other object: not one byte changes, whatever the status */
    memset(other, 0, sizeof other); *(void**)other = otherVt; memcpy(before, other, sizeof other);
    FGCHECK(FGFix_IsOwn(other) == 0);
    for (int s = 0; s < 5; s++) ((PFN)fn)(other, (uint8_t)s);
    FGCHECK(memcmp(before, other, sizeof other) == 0);
    /* the signature: the copy no longer matches (patched), the original bytes do, exactly and up to field offsets */
    static uint8_t shifted[sizeof FGFIX_ORIGINAL];
    memcpy(shifted, FGFIX_ORIGINAL, sizeof shifted);
    shifted[0x1a + 2] = 0xb8;   /* a different field offset for one store */
    FGCHECK(FGFix_Matches(FGFIX_ORIGINAL, 0) && !FGFix_Matches(shifted, 0) && FGFix_Matches(shifted, 1) && !FGFix_Matches(fn, 1));
    _snprintf_s(msg, n, _TRUNCATE, "selftest: %d failure(s) (%s)", fails, why);
    return fails == 0;
}
