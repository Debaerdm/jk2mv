# Build notes

This document explains the first maintainability-oriented map of the repository.

## Top-level targets

The source tree currently builds three main deliverables:

- `jk2mvmp`: the multiplayer client executable,
- `jk2mvded`: the dedicated server executable,
- `jk2mvmenu`: the menu module built from the `mvsdk` submodule.

## Important source areas

- `src/qcommon`: shared engine core, including filesystem, cvars, commands, network messaging, VM, and low-level runtime pieces.
- `src/client`: client runtime, input, UI bridge, demo handling, and audio.
- `src/server`: dedicated server logic.
- `src/renderer`: rendering backend and scene-related code.
- `src/sys`, `src/sdl`, `src/win32`: platform and window/input integration.
- `src/mvsdk`: SDK submodule used for menu and other game-facing pieces.

## Fork strategy

The fork should start with low-risk changes:

1. build clarity,
2. CI simplification where useful,
3. documentation of subsystem ownership,
4. helper extraction in smaller files,
5. only then deeper refactors in core engine areas.

## High-risk areas to avoid early

Do not start with these unless there is a narrow bugfix:

- `src/qcommon/files.cpp`
- `src/qcommon/common.cpp`
- `src/qcommon/msg.cpp`
- `src/qcommon/vm.cpp` and architecture-specific VM backends
- `src/client/cl_main.cpp`
- `src/client/snd_dma.cpp`

## First safe reading order

Recommended order for learning the codebase:

1. `src/CMakeLists.txt`
2. `src/client/cl_console.cpp`
3. `src/client/cl_scrn.cpp`
4. `src/client/cl_demos_auto.cpp`
5. `src/qcommon/cmd.cpp`
6. `src/qcommon/cvar.cpp`
