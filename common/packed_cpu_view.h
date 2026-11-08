#pragma once

#include <cstddef>
#include <cstdint>
#include "cpu_state.h"
#include "runtime_arm_types.h"

namespace armv4t {

// A field is a value-writing reference, including assignment from another field.
// There are no host bitfield-layout or type-punning assumptions.
template<unsigned Shift, uint32_t Mask = 1>
class PackedPsrField {
    uint32_t& word_;
public:
    explicit PackedPsrField(uint32_t& word) : word_(word) {}
    operator uint32_t() const { return (word_ >> Shift) & Mask; }
    PackedPsrField& operator=(uint32_t value) {
        if constexpr (Mask == 1) value = uint32_t(value != 0);
        word_ = (word_ & ~(Mask << Shift)) | ((value & Mask) << Shift);
        return *this;
    }
    PackedPsrField& operator=(const PackedPsrField& source) {
        return *this = uint32_t(source);
    }
};

class PackedCpsrView {
    uint32_t word_;
public:
    static constexpr uint32_t modeled_mask = 0xf80000ffu;
    PackedPsrField<31> n;
    PackedPsrField<30> z;
    PackedPsrField<29> c;
    PackedPsrField<28> v;
    PackedPsrField<27> q;
    PackedPsrField<7> i;
    PackedPsrField<6> f;
    PackedPsrField<5> t;
    PackedPsrField<0,31> mode;

    explicit PackedCpsrView(uint32_t word)
        : word_(word & modeled_mask), n(word_), z(word_), c(word_), v(word_),
          q(word_), i(word_), f(word_), t(word_), mode(word_) {}
    PackedCpsrView(const PackedCpsrView&) = delete;
    PackedCpsrView& operator=(const PackedCpsrView&) = delete;
    uint32_t value() const { return word_; }
    uint32_t& storage() { return word_; }
    operator CPSR() const {
        CPSR result{};
        result.n=uint32_t(n);result.z=uint32_t(z);result.c=uint32_t(c);
        result.v=uint32_t(v);result.q=uint32_t(q);result.i=uint32_t(i);
        result.f=uint32_t(f);result.t=uint32_t(t);result.mode=uint32_t(mode);
        return result;
    }
};

class PackedRegisterWindow {
    uint32_t* registers_;
    uint32_t pc_;
public:
    explicit PackedRegisterWindow(uint32_t (&registers)[16])
        : registers_(registers), pc_(registers[15]) {}
    uint32_t& operator[](std::size_t index) {
        return index == 15 ? pc_ : registers_[index];
    }
    const uint32_t& operator[](std::size_t index) const {
        return index == 15 ? pc_ : registers_[index];
    }
};

// Active R0-R14 and bank arrays are canonical storage shared with the caller.
// PC/CPSR are a small instruction-control projection: publish at the caller's
// existing observation boundaries. Bus callbacks retain the published PC/control
// while multi-access instructions may expose partial general-register updates.
// This view is not a value snapshot and cannot be copied or moved.
class PackedCpuStateView {
    ArmCpuState& state_;
public:
    PackedRegisterWindow R;
    PackedCpsrView cpsr;
    uint32_t (&banked_sp)[ARM_BANK_COUNT];
    uint32_t (&banked_lr)[ARM_BANK_COUNT];
    uint32_t (&banked_spsr)[ARM_BANK_COUNT];
    uint32_t (&r8_12_user)[5];
    uint32_t (&r8_12_fiq)[5];
    PackedPsrField<5> thumb;

    explicit PackedCpuStateView(ArmCpuState& state)
        : state_(state), R(state.R), cpsr(state.cpsr),
          banked_sp(state.banked_sp), banked_lr(state.banked_lr),
          banked_spsr(state.banked_spsr), r8_12_user(state.r8_12_user),
          r8_12_fiq(state.r8_12_fiq), thumb(cpsr.storage()) {
        static_assert(ARM_BANK_COUNT == Bank_Count);
    }
    PackedCpuStateView(const PackedCpuStateView&) = delete;
    PackedCpuStateView& operator=(const PackedCpuStateView&) = delete;
    void publish_control() const {
        state_.R[15] = R[15];
        state_.cpsr = cpsr.value();
    }
};

} // namespace armv4t
