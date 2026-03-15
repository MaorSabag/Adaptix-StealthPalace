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