# Crystal Palace RDLL Template for Adaptix C2

A Crystal Palace-based reflective loader pipeline for Adaptix agents, with runtime API hook support, sleep obfuscation, resource masking, and section-aware memory protection restoration.

## Purpose

This repository contains the loader side of the integration:

1. Build Adaptix agent wrappers (`exe`, `dll`, `svc`) from a generated PIC blob.
2. Hook selected APIs through IAT-compatible call paths.
3. Obfuscate in-memory image during sleep cycles.
4. Restore section permissions after wake-up for safer execution.
5. Keep payloads masked in the embedded resources until runtime.

This repository does not contain the full Adaptix source tree. Adaptix-side compatibility patches are tracked in a separate fork/branch.

## Project Layout

```text
install.sh                 # Automated install: build, patch, register extender
Makefile                   # COFF build and utility targets
src/                       # Core PIC loader/hook logic
loader/                    # EXE/DLL/SVC wrappers and includes
src_service/               # Adaptix builder extender plugin integration
crystal_palace/            # Crystal Palace linker/spec toolchain assets
bin/                       # Build artifacts
demo/                      # Optional demo assets
```

## Installation

### Automated (recommended)

`install.sh` handles everything in one shot: builds the project, patches the root path in `pl_agent.go`, marks Crystal Palace tools executable, builds the service extender plugin, and registers it in the Adaptix `profile.yaml`.

```bash
chmod +x install.sh
./install.sh --ax /path/to/AdaptixServer
```

`--ax` must point to the directory that contains `profile.yaml` (your Adaptix server root). The script will fail early with a clear message if the path is wrong or the profile is missing.

After it completes, restart your Adaptix teamserver to pick up the new extender.

---

### Manual Setup

If you prefer to set things up step by step, install the dependencies first.

#### System Packages

Install the following on your build host (Debian/Ubuntu example):

```bash
# Java runtime (required by Crystal Palace toolchain)
sudo apt install -y default-jre

# MinGW cross-compiler (COFF compilation)
sudo apt install -y gcc-mingw-w64-x86-64

# Clang/LLVM (final wrapper compilation)
sudo apt install -y clang lld

# Go (building the Adaptix service extender plugin)
# Requires Go 1.25+ — install from https://go.dev/dl/

# Build essentials
sudo apt install -y make
```

#### Crystal Palace Toolchain

The toolchain ships in `crystal_palace/` and includes:

| File | Purpose |
|---|---|
| `crystalpalace.jar` | Core linker/PIC builder (Java) |
| `coffparse` | COFF object parser (bash wrapper → Java) |
| `link` | Spec-driven linker (bash wrapper → Java) |
| `piclink` | PIC blob builder (bash wrapper → Java) |
| `disassemble` | Disassembler (bash wrapper → Java) |
| `libtcg.x64.zip` | TCG support library — unzip into place |
| `specs/` | Linker spec files (`loader.spec`, `pico.spec`, `services.spec`) |

Make the shell wrappers executable:

```bash
chmod +x crystal_palace/coffparse crystal_palace/link crystal_palace/piclink crystal_palace/disassemble
```

#### Build

```bash
# Build COFF objects
make

# Build the service extender plugin
cd src_service && make
```

Then manually add `src_service/dist/config.yaml` to the `extenders:` list in your Adaptix `profile.yaml`.

#### Quick Dependency Check

```bash
java -version           # Java 11+
x86_64-w64-mingw32-gcc --version
clang++ --version
go version              # 1.25+
make --version
```

## Adaptix Source Compatibility

The Adaptix-side changes are maintained in this branch:

- https://github.com/MaorSabag/AdaptixC2/tree/Compatible-with-StealthPalace

### Using the pre-patched branch (recommended)

The easiest approach — clone or switch to the branch that already has all changes applied:

```bash
git clone -b Compatible-with-StealthPalace https://github.com/MaorSabag/AdaptixC2.git
```

Or if you already have the repo:

```bash
git remote add stealthpalace https://github.com/MaorSabag/AdaptixC2.git
git fetch stealthpalace
git checkout -b stealthpalace stealthpalace/Compatible-with-StealthPalace
```

### Manual port (advanced)

If you prefer to apply the changes on top of upstream `Adaptix-Framework/AdaptixC2:main`, cherry-pick these commits in order:

```bash
git remote add stealthpalace https://github.com/MaorSabag/AdaptixC2.git
git fetch stealthpalace Compatible-with-StealthPalace

git cherry-pick 5e2af22  # SMB Connector → event-driven blocking
git cherry-pick 4d977de  # SMB connector follow-up
git cherry-pick c463915  # Minor fixes
git cherry-pick 2b7c3c7  # Final result
git cherry-pick ac0ab9e  # AdaptixServer changes
```

If a cherry-pick conflicts, refer to the full commit for context:

| Commit | Description | Link |
|---|---|---|
| `5e2af22` | Restructure SMB Connector from Polling to Event-Driven Blocking | [view](https://github.com/MaorSabag/AdaptixC2/commit/5e2af220bd407e72d7349f9d726ad6a99c0bd38d) |
| `4d977de` | SMB connector follow-up | [view](https://github.com/MaorSabag/AdaptixC2/commit/4d977dee51dc7dca9ce3ec43af42e52b94305ac1) |
| `c463915` | Minor fixes | [view](https://github.com/MaorSabag/AdaptixC2/commit/c463915249ace510e9874d28911b33c50687855e) |
| `2b7c3c7` | Final result | [view](https://github.com/MaorSabag/AdaptixC2/commit/2b7c3c76e5a06c7f12d00955bfe3fe7b04c6a978) |
| `ac0ab9e` | AdaptixServer changes | [view](https://github.com/MaorSabag/AdaptixC2/commit/ac0ab9ebbdf0d8d4b9831bddd5386d20b3e1b19e) |

<details>
<summary>What these commits change</summary>

- SMB connector transition from polling to event-driven/overlapped flow.
- Connector and agent-side API/interface alignment for response handling.
- Follow-up fixes for connector timing and stability.
- Consolidated compatibility edits across loader-facing agent code.
- Adaptix server-side post-build hook changes for StealthPalace wrapping.

**Adaptix files typically touched:**

- `AdaptixServer/teamserver/evt/evt_types.go`
- `AdaptixServer/teamserver/extender/ts_agent_builder.go`
- `AdaptixServer/template/implant/src/core/ApiLoader.cpp`
- `AdaptixServer/template/implant/src/core/ApiLoader.h`
- `AdaptixServer/template/implant/src/core/ApiDefines.h`
- `AdaptixServer/template/implant/src/core/ConnectorSMB.cpp`
- `AdaptixServer/template/implant/src/core/ConnectorSMB.h`
- `AdaptixServer/template/implant/src/core/MainAgent.cpp`
- `AdaptixServer/template/implant/src/core/Pivotter.cpp`
- `AdaptixServer/template/implant/src/core/Pivotter.h`

</details>

## Compiler Notes

- `-mno-stack-arg-probe`: avoids `___chkstk_ms` relocation issues in deep obfuscation paths.
- `-fno-zero-initialized-in-bss`: keeps globals in relocatable sections for PIC usage.

## Demo

https://github.com/user-attachments/assets/240e1b2d-c8f1-4e70-865d-872f04e192a9

## Credits

- Crystal Palace RDLL approach by Raphael Mudge
- Ekko research by C5pider
- Adaptix C2 framework by Adaptix-Framework
- Kharon Agent inspiration for loader patterns
- Original Adaptix Crystal Palace template by h41th

## Disclaimer

For authorized security testing and red-team operations only. Ensure you have explicit permission before use.
