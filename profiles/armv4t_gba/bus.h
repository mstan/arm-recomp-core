// bus.h — Portable memory-bus interface used by the IR interpreter
// and by codegen-emitted helpers.
//
// This lives in the armv4t/ layer so the interpreter can stay
// platform-portable (no gba/ dependency). The concrete GBA bus
// (`gba::MemoryBus`) inherits from this — see `src/gba/gba_memory.h`.
//
// The interface deliberately exposes only the read/write primitives.
// Unmapped-access logging, region classification, waitstate timing,
// IO-side-effects — all of that lives in concrete implementations.

#pragma once

#include <cstdint>

namespace armv4t {

struct Bus {
    virtual ~Bus() = default;

    virtual uint8_t  read8 (uint32_t addr) = 0;
    virtual uint16_t read16(uint32_t addr) = 0;
    virtual uint32_t read32(uint32_t addr) = 0;

    virtual void write8 (uint32_t addr, uint8_t  v) = 0;
    virtual void write16(uint32_t addr, uint16_t v) = 0;
    virtual void write32(uint32_t addr, uint32_t v) = 0;

    // Cycles taken by a memory access at `addr` of `width` bytes (1/2/4).
    // `sequential` distinguishes ARM7TDMI's "S" (sequential, same area
    // as previous access) from "N" (non-sequential / first access of a
    // burst). For each region the concrete bus returns the proper
    // S/N-cycle count derived from bus width + waitstates. Default
    // implementation returns 1 (single-cycle, zero-waitstate model)
    // so callers that don't override get the previous behaviour.
    virtual uint32_t access_cycles(uint32_t /*addr*/, uint8_t /*width*/,
                                   bool /*sequential*/) const {
        return 1;
    }

    // Opcode-fetch wait states: cycles added on top of the base 1 for one
    // fetch from the code region containing `pc`, 16-bit for THUMB and
    // 32-bit for ARM. `instr_cycle_base` charges every fetch as a
    // zero-wait 1S, so the interpreter and the codegen add
    //   + code_wait(pc, S)                        every instruction
    //   + code_wait(pc, N) - code_wait(pc, S)     after a data access or
    //                                             multiply (next fetch is N)
    //   + code_wait(dst, N) + code_wait(dst, S)   after any PC write
    // Default 0 keeps a zero-wait bus (IWRAM/BIOS behaviour everywhere).
    virtual uint32_t code_wait(uint32_t /*pc*/, bool /*thumb*/,
                               bool /*sequential*/) const {
        return 0;
    }

    // GamePak prefetch buffer. `wait` is the stall a data access (or the
    // internal cycles of a multiply) adds while the instruction at `pc`
    // executes; the concrete bus returns the adjusted stall (it may be
    // negative: prefetched opcodes make later fetches free). The default
    // has no prefetch buffer.
    virtual int32_t prefetch_stall(int32_t wait, uint32_t /*pc*/,
                                   bool /*thumb*/) {
        return wait;
    }
};

}  // namespace armv4t
