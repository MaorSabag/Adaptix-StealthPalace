# StealthPalace

> **Reflective DLL Loader & Sleep Obfuscation Engine for [Adaptix C2](https://github.com/Adaptix-Framework/AdaptixC2)**

StealthPalace is a production-grade RDLL loader built on [Crystal Palace](https://www.cobaltstrike.com/product/crystal-palace) that keeps the Adaptix agent dark between callbacks. It combines IAT-level hooking, RC4-based Ekko sleep obfuscation, overlapped I/O for the SMB beacon, and per-section memory permission restoration — all compiled into a position-independent PIC blob that cleans itself up after execution.

---

## Core Capabilities

| Capability | Details |
|---|---|
| **XOR Resource Masking** | Payload encrypted at link time with a random 128-byte key via Crystal Palace directives. Decrypted at runtime into a temporary `VirtualAlloc` buffer, mapped, then securely wiped. |
| **Ekko Sleep Obfuscation** | Full image RC4-encrypted in memory during sleep via a 6-step `NtContinue` ROP chain (timer queue callbacks). Triggered on `Sleep`, `ConnectNamedPipe`, `FlushFileBuffers`, and `WaitForSingleObjectEx`. |
| **IAT Hooking via PICO** | PICO intercepts `GetProcAddress` at load time to redirect target APIs through the hook table, enabling transparent sleep obfuscation without modifying agent source flow. |
| **Per-Section Permission Restore** | After decryption, a PE section walker applies correct page protections (`.text` → `RX`, `.data` → `RW`, etc.) instead of blanket `RWX`. Required for BOF compatibility and cleaner memory forensics. |
| **Overlapped I/O SMB Beacon** | Named pipe opened with `FILE_FLAG_OVERLAPPED`. `WaitForSingleObjectEx(INFINITE, TRUE)` acts as the unified sleep hook point, eliminating `PeekNamedPipe` polling. |
| **PIC Self-Cleanup** | After `go()` returns, the loader's own RX allocation is wiped and freed — no artifacts left on the heap. |

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                     StealthPalace PIC Blob                  │
│                                                             │
│  ┌──────────┐    ┌──────────┐    ┌──────────────────────┐   │
│  │ loader.c │───▶│  pico.c  │───▶│  Adaptix Agent DLL   │   │
│  │          │    │          │    │  (mapped in memory)  │   │
│  │ XOR-dec  │    │ hook IAT │    │                      │   │
│  │ map DLL  │    │ GetProc  │    │  Sleep()─────────────┼───┼──▶ hooks.c
│  │ fix perms│    │ Address  │    │  ConnectNamedPipe()──┼───┼──▶ EkkoObf()
│  └──────────┘    └──────────┘    │  WaitForSingleObj()──┼───┼──▶ RC4 enc/dec
│                                  └──────────────────────┘   │  + perm restore
│  ┌──────────────┐                                           │
│  │ services.c   │  API resolution via ROR13 hash walking    │
│  └──────────────┘                                           │
└─────────────────────────────────────────────────────────────┘
```

### Sleep Obfuscation — ROP Chain Detail

When any hooked sleep function is called, `EkkoObf()` queues 6 timer callbacks that execute sequentially via `NtContinue`, implementing the Ekko technique:

```
1. NtContinue → RtlCaptureContext      (save current CONTEXT)
2. NtContinue → SetEvent               (signal start)
3. NtContinue → SystemFunction032      (RC4-encrypt image in-place)
4. NtContinue → WaitForSingleObjectEx  (actual sleep — alertable wait)
5. NtContinue → SystemFunction032      (RC4-decrypt image)
6. NtContinue → restore_section_perms  (re-apply RX/RW/RO per section)
```

The RC4 key is derived from a stack-allocated context, not stored statically. The image is opaque to EDR memory scanners during the wait window.

---

## Project Layout

```text
src/
  loader.c          # Main PIC: XOR-unmasks DLL, loads PICO, maps DLL, fixes permissions, calls entry
  hooks.c           # IAT hooks → EkkoObf() + restore_section_permissions()
  pico.c            # PICO: hooked GetProcAddress, setup_hooks(), set_image_info()
  services.c        # API resolution via ROR13 hash walking
  loader.h          # PE export lookup helper
  stomp.c / stomp.h # Module stomping support
  tcg.h             # Crystal Palace intrinsics & DLL loader structs

crystal_palace/
  specs/
    loader.spec     # Main PIC build spec (XOR masking, key generation)
    pico.spec       # PICO build spec (merges hooks.c, registers Sleep hook)
    services.spec   # Services build spec (API resolution via ror13/strings)
  link              # Crystal Palace link wrapper
  piclink           # Crystal Palace PIC link wrapper
  coffparse         # COFF parser utility
  disassemble       # Disassembler utility

loader/
  include/
    Adaptix.h       # Adaptix C2 header
    Shellcode.h     # Auto-generated PIC blob as C hex array
  source/main/
    Exe.cc          # Final binary wrapper (EXE format)
    Dll.cc          # Final binary wrapper (DLL format)
    Svc.cc          # Final binary wrapper (service format)
  test/
    run.c           # Test harness (loads and executes the PIC blob)

src_service/        # Adaptix builder extender plugin (Go)
install.sh          # Automated install: build, patch, register extender
Makefile            # COFF object compilation
bin/                # Compiled COFF objects and PIC blob (agent.bin)
```

---

## Prerequisites

- Crystal Palace toolchain (included in `crystal_palace/`)
- MinGW cross-compiler: `x86_64-w64-mingw32-gcc`
- Go 1.25+ (for the Adaptix service extender plugin)
- Clang: `clang++` targeting `x86_64-w64-mingw32` (final binary compilation)
- Java 11+ (required by Crystal Palace)

---

## Installation

### Automated (recommended)

`install.sh` handles everything in one shot: builds the project, patches the root path in `pl_agent.go`, marks Crystal Palace tools executable, builds the service extender plugin, and registers it in the Adaptix `profile.yaml`.

```bash
chmod +x install.sh
./install.sh --ax /path/to/AdaptixServer
```

`--ax` must point to the directory containing `profile.yaml` (your Adaptix server root). The script will fail early with a clear message if the path is wrong or the profile is missing.

After it completes, restart your Adaptix teamserver to pick up the new extender.

---

### Manual Setup

Install the following on your build host (Debian/Ubuntu example):

```bash
sudo apt install -y default-jre gcc-mingw-w64-x86-64 clang lld make
# Go 1.25+ — install from https://go.dev/dl/
```

Make the Crystal Palace shell wrappers executable:

```bash
chmod +x crystal_palace/coffparse crystal_palace/link crystal_palace/piclink crystal_palace/disassemble
```

Build:

```bash
# Build COFF objects
make

# Build the service extender plugin
cd src_service && make
```

Then manually add `src_service/dist/config.yaml` to the `extenders:` list in your Adaptix `profile.yaml`.

#### Quick Dependency Check

```bash
java -version                        # Java 11+
x86_64-w64-mingw32-gcc --version
clang++ --version
go version                           # 1.25+
make --version
```

---

## Adaptix Source Compatibility

StealthPalace requires specific patches to the Adaptix agent source. The patched branch is maintained at:

- **https://github.com/MaorSabag/AdaptixC2/tree/Compatible-with-StealthPalace**

### Using the pre-patched branch (recommended)

```bash
git clone -b Compatible-with-StealthPalace https://github.com/MaorSabag/AdaptixC2.git
```

Or if you already have the repo:

```bash
git remote add stealthpalace https://github.com/MaorSabag/AdaptixC2.git
git fetch stealthpalace
git checkout -b stealthpalace stealthpalace/Compatible-with-StealthPalace
```
---

## Resource Masking — Crystal Palace Directives

The embedded DLL payload is never stored in cleartext inside the PIC blob. `loader.spec` handles masking at link time:

```
generate $KEY 128       # random 128-byte XOR key

push $DLL
    xor $KEY            # XOR-encrypt the DLL
    preplen             # prepend cleartext length
    link "dll"          # embed into the "dll" section

push $KEY
    preplen             # prepend key length
    link "mask"         # embed into the "mask" section
```

At runtime, `loader.c` reads both sections as length-prefixed `RESOURCE` blobs, XOR-decrypts into a temporary `VirtualAlloc` buffer, maps the PE into a correctly laid-out image, then wipes and frees the plaintext copy.

---

## Compiler Flags

| Flag | Rationale |
|---|---|
| `-mno-stack-arg-probe` | Avoids `___chkstk_ms` relocation. EkkoObf uses ~8.5 KB of stack — the probe would fault inside the ROP chain. |
| `-fno-zero-initialized-in-bss` | Forces zero-initialized globals into `.data`. PIC blobs cannot resolve `.bss` relocations. |

---

## Demo

https://github.com/user-attachments/assets/240e1b2d-c8f1-4e70-865d-872f04e192a9

---

## Credits

- [h41th](https://github.com/h41th/Simple-Crystal-Palace-RDLL-template-for-Adaptix) — original Crystal Palace loader template for Adaptix
- [Raphael Mudge](https://www.cobaltstrike.com) — Crystal Palace toolchain
- [C5pider](https://github.com/Cracked5pider/Ekko) — Ekko sleep obfuscation
- [Adaptix-Framework](https://github.com/Adaptix-Framework/AdaptixC2) — Adaptix C2 framework
- [entropy-z](https://github.com/entropy-z/Kharon) — Kharon agent loader inspiration

---

## Disclaimer

This project is intended for **authorized red team operations, security research, and educational use only**. Do not deploy against systems you do not own or have explicit written permission to test.
