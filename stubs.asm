; version.dll exports of the SWZC Frame Generation crash fix: each jumps to the real System32 function (pointers set in DllMain).
; Jump stubs keep every argument and the stack untouched, whatever the prototype.

STUB MACRO name
EXTERN gm_p_&name&:QWORD
gm_&name& PROC
    jmp QWORD PTR [gm_p_&name&]
gm_&name& ENDP
ENDM

.code
STUB GetFileVersionInfoA
STUB GetFileVersionInfoByHandle
STUB GetFileVersionInfoExA
STUB GetFileVersionInfoExW
STUB GetFileVersionInfoSizeA
STUB GetFileVersionInfoSizeExA
STUB GetFileVersionInfoSizeExW
STUB GetFileVersionInfoSizeW
STUB GetFileVersionInfoW
STUB VerFindFileA
STUB VerFindFileW
STUB VerInstallFileA
STUB VerInstallFileW
STUB VerQueryValueA
STUB VerQueryValueW
END
