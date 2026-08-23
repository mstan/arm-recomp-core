# arm-recomp-core

Shared ARM recompiler architecture core used by recompilation projects.

This repository is a source package, not a standalone runtime. Consumers own
their platform runtime ABI, memory map, generated dispatch tables, scheduler,
and device models. The shared core owns architecture-facing pieces such as ARM
and Thumb decode, normalized IR, the reference interpreter, shared CPU state
types, and profile-specific code generation.

## CMake

Consumers include `cmake/ArmRecompCore.cmake` and request sources/include paths:

```cmake
set(ARM_RECOMP_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/arm-recomp-core")
include("${ARM_RECOMP_CORE_DIR}/cmake/ArmRecompCore.cmake")
arm_recomp_core_sources(ARM_RECOMP_CORE_SOURCES armv5te_nds)
arm_recomp_core_include_dirs(ARM_RECOMP_CORE_INCLUDE_DIRS armv5te_nds)
```

Profiles:

- `armv4t`: common decode/IR/interpreter sources.
- `armv5te_nds`: common sources plus the current NDS ARMv5TE codegen profile.

The NDS runtime ABI (`runtime_arm.h`, dual-CPU dispatch, CP15, live overlays,
and bus fast paths) remains consumer-owned.
