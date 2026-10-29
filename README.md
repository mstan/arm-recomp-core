# arm-recomp-core

Shared ARM recompiler architecture core used by ARM-based recompilation
projects.

This repository is a source package, not a standalone runtime. Consumers own
their platform runtime ABI, memory map, generated dispatch tables, scheduler,
and device models. The shared core owns architecture-facing pieces such as ARM
and Thumb decode, normalized IR, the reference interpreter, shared CPU state
types, and profile-specific code generation.

## Consumers

This core is intended to be shared by:

- [ndsrecomp](https://github.com/mstan/ndsrecomp) for Nintendo DS ARM9/ARM7
  static recompilation and runtime validation.
- [gbarecomp](https://github.com/mstan/gbarecomp) for Game Boy Advance ARM7TDMI
  recompilation.

The repository boundary mirrors the `m68k-recomp-core` pattern used by the
68k-based recompilers: architecture code lives here, while each consumer keeps
its system-specific runtime, generated-bank registration, device model, and
release packaging.

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
- `armv4t_gba`: GBA ARM7TDMI decode/IR/interpreter sources plus the GBA
  codegen profile.
- `armv5te_nds`: common sources plus the current NDS ARMv5TE codegen profile.

Runtime ABIs remain consumer-owned. NDS keeps `runtime_arm.h`, dual-CPU
dispatch, CP15, live overlays, and bus fast paths in `ndsrecomp`; GBA keeps its
runtime dispatch, symbol registration, overlay shims, and GBA bus/device model
in `gbarecomp`.

## Interpreter storage contracts

The common interpreter retains the structured `CPUState` API. An overload
accepts `PackedCpuStateView` over the shared C ABI `ArmCpuState`, using the
same instruction implementation. The view aliases R0-R14 and bank arrays;
PC and modeled CPSR bits are projected until `publish_control()` is called.
Callers must publish at their existing observation boundaries. Bus callbacks
see that published PC/CPSR, while multi-access instructions can expose partial
general-register updates through canonical storage. The view must not outlive
its backing state and cannot be copied or moved.

The packed control projection drops unmodeled CPSR bits, matching the
structured API. This is a caller-selected storage contract, not a new CPU
model, timing model or automatic publication policy. The GBA profile remains
independent.

Configure with `-DARM_RECOMP_CORE_BUILD_TESTS=ON` to run `packed_state_test`
through CTest. It compares both common storage contracts across all Thumb
encodings, deterministic ARM samples, exception banks and bus control/cycle
transcripts. Consumer scheduler/device integration remains a consumer test.
