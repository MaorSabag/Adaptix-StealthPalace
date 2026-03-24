#pragma once
#include <windows.h>
#include "tcg.h"

DECLSPEC_IMPORT BOOL    WINAPI KERNEL32$VirtualProtect(LPVOID, SIZE_T, DWORD, PDWORD);
DECLSPEC_IMPORT BOOL    WINAPI KERNEL32$VirtualFree(LPVOID, SIZE_T, DWORD);
DECLSPEC_IMPORT HMODULE WINAPI KERNEL32$LoadLibraryExA(LPCSTR, HANDLE, DWORD);
DECLSPEC_IMPORT void*  __cdecl MSVCRT$memset(void*, int, size_t);
DECLSPEC_IMPORT NTSYSAPI BOOLEAN WINAPI KERNEL32$RtlAddFunctionTable(PRUNTIME_FUNCTION, DWORD, DWORD64);
DECLSPEC_IMPORT NTSYSAPI BOOLEAN WINAPI KERNEL32$RtlDeleteFunctionTable(PRUNTIME_FUNCTION);

/* Shared by both code paths */
typedef struct _SP_UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} SP_UNICODE_STRING;

#if defined(STOMP_TECHNIQUE) && STOMP_TECHNIQUE == 1

typedef struct _SP_OBJECT_ATTRIBUTES {
    ULONG              Length;
    HANDLE             RootDirectory;
    SP_UNICODE_STRING* ObjectName;
    ULONG              Attributes;
    PVOID              SecurityDescriptor;
    PVOID              SecurityQualityOfService;
} SP_OBJECT_ATTRIBUTES;

typedef struct _SP_IO_STATUS_BLOCK {
    union { NTSTATUS Status; PVOID Pointer; };
    ULONG_PTR Information;
} SP_IO_STATUS_BLOCK;

#define SP_NT_SUCCESS(s)        (((NTSTATUS)(s)) >= 0)
#define SP_OBJ_CASE_INSENSITIVE 0x40UL
#define SP_VIEW_SHARE           1UL

#define SP_INIT_OBJ_ATTR(o, n, a) do { \
    (o).Length                   = sizeof(SP_OBJECT_ATTRIBUTES); \
    (o).RootDirectory            = NULL; \
    (o).ObjectName               = (n); \
    (o).Attributes               = (a); \
    (o).SecurityDescriptor       = NULL; \
    (o).SecurityQualityOfService = NULL; \
} while (0)

DECLSPEC_IMPORT NTSTATUS NTAPI NTDLL$NtOpenFile(PHANDLE, ACCESS_MASK, PVOID, PVOID, ULONG, ULONG);
DECLSPEC_IMPORT NTSTATUS NTAPI NTDLL$NtCreateSection(PHANDLE, ACCESS_MASK, PVOID, PVOID, ULONG, ULONG, HANDLE);
DECLSPEC_IMPORT NTSTATUS NTAPI NTDLL$NtMapViewOfSection(HANDLE, HANDLE, PVOID*, ULONG_PTR, SIZE_T, PVOID, PSIZE_T, ULONG, ULONG, ULONG);
DECLSPEC_IMPORT NTSTATUS NTAPI NTDLL$NtUnmapViewOfSection(HANDLE, PVOID);
DECLSPEC_IMPORT NTSTATUS NTAPI NTDLL$NtClose(HANDLE);

DECLSPEC_IMPORT LPVOID WINAPI KERNEL32$VirtualAlloc(LPVOID, SIZE_T, DWORD, DWORD);

#endif /* STOMP_TECHNIQUE == 1 */


typedef struct _SP_LIST_ENTRY {
    struct _SP_LIST_ENTRY* Flink;
    struct _SP_LIST_ENTRY* Blink;
} SP_LIST_ENTRY, *PSP_LIST_ENTRY;

typedef struct _SP_LDR_DATA_TABLE_ENTRY {
    SP_LIST_ENTRY     InLoadOrderLinks;            
    SP_LIST_ENTRY     InMemoryOrderLinks;          
    SP_LIST_ENTRY     InInitializationOrderLinks;  
    PVOID             DllBase;                     
    PVOID             EntryPoint;                  
    ULONG             SizeOfImage;                 
    ULONG             _pad_044;                    
    SP_UNICODE_STRING FullDllName;                 
    SP_UNICODE_STRING BaseDllName;                 
    ULONG             Flags;                       
    USHORT            LoadCount;                   
    USHORT            TlsIndex;                    
    SP_LIST_ENTRY     HashLinks;                   
    ULONG             TimeDateStamp;               
    ULONG             _pad_084;                    
    PVOID             EntryPointActivationContext; 
    PVOID             Lock;                        
    PVOID             DdagNode;                    
    SP_LIST_ENTRY     NodeModuleLink;              
    PVOID             LoadContext;                 
    PVOID             ParentDllBase;               
} SP_LDR_DATA_TABLE_ENTRY, *PSP_LDR_DATA_TABLE_ENTRY;

typedef struct _SP_LDR_DDAG_NODE {
    SP_LIST_ENTRY Modules;                 
    SP_LIST_ENTRY ServiceTagList;          
    ULONG         LoadCount;               
    ULONG         LoadWhileUnloadingCount; 
    ULONG         LowestLink;              
    ULONG         _pad02C;                 
    PVOID         Dependencies;            
    PVOID         IncomingDependencies;    
    LONG          State;                   
    ULONG         _pad044;                 
    PVOID         CondenseLink;            
    ULONG         PreorderNumber;          
    ULONG         _pad054;                 
} SP_LDR_DDAG_NODE, *PSP_LDR_DDAG_NODE;

#define SP_LDR_MODULES_READY_TO_RUN 9

typedef struct _SP_PEB_LDR_DATA {
    ULONG         Length;
    BYTE          Initialized;
    BYTE          _Pad0[3];
    PVOID         SsHandle;
    SP_LIST_ENTRY InLoadOrderModuleList;
    SP_LIST_ENTRY InMemoryOrderModuleList;
    SP_LIST_ENTRY InInitializationOrderModuleList;
} SP_PEB_LDR_DATA, *PSP_PEB_LDR_DATA;

typedef struct _SP_PEB {
    BYTE             InheritedAddressSpace;
    BYTE             ReadImageFileExecOptions;
    BYTE             BeingDebugged;
    BYTE             BitField;
    BYTE             _Pad0[4];
    PVOID            Mutant;
    PVOID            ImageBaseAddress;
    PSP_PEB_LDR_DATA Ldr;
} SP_PEB, *PSP_PEB;

#define SP_LDRP_IMAGE_DLL       0x00000004UL
#define SP_LDRP_ENTRY_PROCESSED 0x00004000UL

#define SP_FAKE_LDR_STRINGS_OFFSET 0x150
#define SP_FAKE_LDR_FULL_WCHARS    512
#define SP_FAKE_LDR_BASE_WCHARS    128
#define SP_FAKE_LDR_BLOCK_BYTES  ((DWORD)( \
    SP_FAKE_LDR_STRINGS_OFFSET + \
    SP_FAKE_LDR_FULL_WCHARS * sizeof(WCHAR) + \
    SP_FAKE_LDR_BASE_WCHARS * sizeof(WCHAR) \
))

typedef struct _PICO {
    char data[4096];
    char code[16384];
} PICO;

typedef enum {
    rPICO,
    rDLL,
} RESOURCE_TYPE;

typedef struct _PICO_ARGS {
    PICO**            pico_dst;
    IMPORTFUNCS*      funcs;
    char*             pico_src;
    const char*       sacrificialDll;
    RUNTIME_FUNCTION* pico_rf_storage;
    int*              pico_rf_count;
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