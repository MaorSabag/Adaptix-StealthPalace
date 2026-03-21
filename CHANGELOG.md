# Changelog
All notable changes to this project will be documented in this file.

## [1.0.0] - 2026-02-28
### Added
- All features merged from development branch.
- Initial clean release for forked repository.

## [1.1.0] - 2026-03-08
### Added
- Added post-generation hook for agent file creation in `src_service/ts_agent_build.go`.
- Created forked Github repository for [AdaptixC2](https://github.com/MaorSabag/AdaptixC2) with code modifications to support StealthPalace.

## [1.1.1] - 2026-03-14
### Added
- Update Crystal Palace and specs files according to this [article](https://aff-wg.org/).
- Changed DLL execution via `DLLMAIN` function.

## [1.1.2] - 2026-03-15
### Added
- Added `install.sh` automated installer: builds COFF objects, patches the root path in `pl_agent.go`, marks Crystal Palace tools executable, builds the service extender plugin, and registers it in the Adaptix `profile.yaml` via `--ax <path>`.

### Changed
- Expanded README prerequisites into a full Installation section with an automated path (`install.sh`) and a manual step-by-step path.
- Added Crystal Palace toolchain prerequisites table covering all required packages (Java, MinGW, Clang/LLVM, Go).

## [1.2.0] - 2026-03-20
### Added
- **Sleep Obfuscation UI**: New collapsible "Sleep Obfuscation" group in the compile window lets you enable/disable Ekko at build time without touching source. The selected technique is persisted in settings and passed to the compiler as `-DSLEEP_OBF_EKKO`.
- **DLL Wrapping toggle**: Added "Enable DLL wrapping hook" checkbox to the compiler flags group; defaults to `true` and is saved/restored across sessions.
- **`.pdata` registration**: After reflective loading, the DLL's exception directory is registered with `RtlAddFunctionTable` so unwind info and SEH are fully functional in the beacon.
- New `RtlAddFunctionTable`, `RtlLookupFunctionEntry`, `GetModuleHandleA`, and `RtlCaptureContext` import stubs added to `loader.h`.
- Compiler flags `-fasynchronous-unwind-tables`, `-mabi=ms`, and `-foptimize-sibling-calls` are now always applied to produce correct unwind tables.
- **Stomp technique**: New options for stomping PICO and Agent's DLL
  - LoadLibraryEx (DONT_RESOLVE_DLL_REFERENCES) Loads the sacrificial DLL without resolving imports. Simple and reliable. However, the DLL is still subject to CFG and ETW callbacks, and a PEB loader entry is automatically created.
  - NtCreateSection + NtMapViewOfSection Maps the DLL directly via NT APIs, synthetic LDR_DATA_TABLE_ENTRY is manually inserted into InLoadOrderModuleList and InMemoryOrderModuleList so stack walkers resolve the DLL name correctly.

### Changed
- **Stack improvements when not sleeping**: All Ekko-specific logic (ROP helpers, `EkkoObf`, `DetourWaitForMultipleObjects`, `RndThreadId`) is now gated behind `#ifdef SLEEP_OBF_EKKO`. Hook functions (`_WaitForSingleObjectEx`, `_WaitForMultipleObjects`, `_ConnectNamedPipe`) pass through cleanly to the real API when sleep obfuscation is disabled, instead of erroring or calling undefined code.
- Ekko dispatch extracted into a dedicated `__attribute__((noinline))` helper (`_WaitForSingleObjectEx_Obf`) to keep the hot-path stack frame minimal and predictable regardless of sleep state.
- Hook functions marked `__attribute__((optimize("O2"), noinline))` for consistent, small stack frames.
- `FlushInstructionCache` is now called **before** `DLL_PROCESS_ATTACH` (previously it was called after, which could lead to stale I-cache on the entry point).
- `Settings` global in `pl_main.go` is now initialized via `defaultSaveSettings()` to guarantee sane defaults even before a config file is loaded.
- Stomp mode allocates a dedicated `RUNTIME_FUNCTION` buffer for pico module entries, passed through `PICO_ARGS`.
- Ekko-specific function resolution (`NtContinue`, `RtlCaptureContext`, `SystemFunction032`) is only performed at startup when `SLEEP_OBF_EKKO` is defined, avoiding unnecessary `LoadLibrary` calls otherwise.