// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>

#if defined(_MSC_VER)
#define ARM_RECOMP_ALU_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define ARM_RECOMP_ALU_INLINE inline __attribute__((always_inline))
#else
#define ARM_RECOMP_ALU_INLINE inline
#endif

namespace arm_recomp::alu {
enum class ShiftKind : std::uint8_t { Lsl, Lsr, Asr, Ror, Rrx };
struct ShiftResult { std::uint32_t value, carry; };
struct AddResult {
    std::uint32_t value = 0;
    bool carry = false, overflow = false;
};
constexpr std::uint32_t rotate_right(std::uint32_t value, unsigned amount) noexcept {
    amount &= 31u;
    return amount ? (value >> amount) | (value << (32u - amount)) : value;
}
ARM_RECOMP_ALU_INLINE constexpr std::uint32_t arithmetic_shift_right(std::uint32_t value, unsigned amount) noexcept {
    if (!amount) return value;
    if (amount >= 32u) return (value >> 31u) ? 0xffff'ffffu : 0u;
    const auto shifted = value >> amount;
    return (value >> 31u) ? shifted | (0xffff'ffffu << (32u - amount)) : shifted;
}
// Width is 1..32. The final conversion is in range on every supported host.
constexpr std::int32_t sign_extend(std::uint32_t value, unsigned bits) noexcept {
    const auto sign=std::uint64_t{1} << (bits-1u);
    const auto modulus=std::uint64_t{1} << bits;
    const auto normalized=std::uint64_t(value)&(modulus-1u);
    return static_cast<std::int32_t>((normalized&sign)
        ? static_cast<std::int64_t>(normalized)-static_cast<std::int64_t>(modulus)
        : static_cast<std::int64_t>(normalized));
}
// The caller distinguishes immediate encoded-zero shifts from register counts.
// Register-count low-byte selection and carry-preserve action codes stay adapters.
// Keep constant encoding/count policies visible to each CPU caller. In particular,
// immediate shifts must not gain an out-of-line generic dispatch in the hot path.
constexpr ShiftResult shift(std::uint32_t value, ShiftKind kind, std::uint32_t count,
                            bool encoded_zero, bool carry_in) noexcept {
    switch (kind) {
    case ShiftKind::Lsl:
        if (!count) return {value, carry_in ? 1u : 0u};
        if (count < 32u) return {value << count, (value >> (32u-count)) & 1u};
        return {0u, count == 32u ? value & 1u : 0u};
    case ShiftKind::Lsr:
        if (!count && !encoded_zero) return {value, carry_in ? 1u : 0u};
        if (!count || count == 32u) return {0u, value >> 31u};
        if (count < 32u) return {value >> count, (value >> (count-1u)) & 1u};
        return {0u, 0u};
    case ShiftKind::Asr:
        if (!count && !encoded_zero) return {value, carry_in ? 1u : 0u};
        if (!count || count >= 32u) return {arithmetic_shift_right(value,32u),value >> 31u};
        return {arithmetic_shift_right(value,count),(value >> (count-1u)) & 1u};
    case ShiftKind::Ror:
        if (!count) return {value,carry_in ? 1u : 0u};
        {const auto rotated=rotate_right(value,count);return {rotated,rotated >> 31u};}
    case ShiftKind::Rrx:
        return {(value >> 1u) | ((carry_in ? 1u : 0u) << 31u),value & 1u};
    }
    return {value,carry_in ? 1u : 0u};
}
// ARM instruction operand shifts report a carry effect rather than copying CPSR.
// carry_action is 0=preserve, 1=clear, 2=set. Register counts use their low byte.
enum class OperandShiftKind : std::uint8_t { Lsl, Lsr, Asr, Ror };
struct OperandShiftResult { std::uint32_t value, carry_action; };
ARM_RECOMP_ALU_INLINE constexpr OperandShiftResult operand_register_shift(const std::uint32_t value,
    const OperandShiftKind kind, const std::uint32_t count) noexcept {
    const std::uint32_t amount = count & 0xffu;
    std::uint32_t shifted = value;
    std::uint32_t carry_action = 0u;
    if (amount != 0u) {
        switch (kind) {
        case OperandShiftKind::Lsl:  // LSL
            if (amount < 32u) {
                shifted = value << amount;
                carry_action = 1u + ((value >> (32u - amount)) & 1u);
            } else if (amount == 32u) {
                shifted = 0u;
                carry_action = 1u + (value & 1u);
            } else {
                shifted = 0u;
                carry_action = 1u;
            }
            break;
        case OperandShiftKind::Lsr:  // LSR
            if (amount < 32u) {
                shifted = value >> amount;
                carry_action = 1u + ((value >> (amount - 1u)) & 1u);
            } else if (amount == 32u) {
                shifted = 0u;
                carry_action = 1u + (value >> 31u);
            } else {
                shifted = 0u;
                carry_action = 1u;
            }
            break;
        case OperandShiftKind::Asr:  // ASR
            if (amount < 32u) {
                shifted = arithmetic_shift_right(value, amount);
                carry_action = 1u + ((value >> (amount - 1u)) & 1u);
            } else {
                shifted = arithmetic_shift_right(value, 32u);
                carry_action = 1u + (value >> 31u);
            }
            break;
        case OperandShiftKind::Ror: {  // ROR
            const std::uint32_t rotation = amount & 31u;
            shifted = rotate_right(value, rotation);
            carry_action = 1u + (shifted >> 31u);
            break;
        }
        }
    }
    return {shifted, carry_action};
}
// amount is the decoded five-bit immediate field (0..31). Zero has its ISA
// encoding meaning. The nonthrowing carry getter runs once for RRX only.
template<class GetCarry>
ARM_RECOMP_ALU_INLINE constexpr OperandShiftResult operand_immediate_shift(
    std::uint32_t value, OperandShiftKind kind, std::uint32_t amount,
    GetCarry get_carry) noexcept {
    std::uint32_t shifted = value;
    std::uint32_t carry_action = 0u;
    switch (static_cast<unsigned>(kind)) {
    case 0u:  // LSL
        if (amount != 0u) {
            shifted = value << amount;
            carry_action = 1u + ((value >> (32u - amount)) & 1u);
        }
        break;
    case 1u:  // LSR; an immediate zero encodes 32.
        shifted = amount == 0u ? 0u : value >> amount;
        carry_action =
            1u + (amount == 0u ? value >> 31u
                               : (value >> (amount - 1u)) & 1u);
        break;
    case 2u:  // ASR; an immediate zero encodes 32.
        shifted = arithmetic_shift_right(value, amount == 0u ? 32u : amount);
        carry_action =
            1u + (amount == 0u ? value >> 31u
                               : (value >> (amount - 1u)) & 1u);
        break;
    case 3u:  // ROR; an immediate zero encodes RRX.
        if (amount == 0u) {
            shifted = (get_carry() ? 0x8000'0000u : 0u) |
                      (value >> 1u);
            carry_action = 1u + (value & 1u);
        } else {
            shifted = rotate_right(value, amount);
            carry_action = 1u + (shifted >> 31u);
        }
        break;
    }
    return {shifted,carry_action};
}

constexpr AddResult add_with_carry(std::uint32_t left, std::uint32_t right,
                                   std::uint32_t carry_in) noexcept {
    const auto sum=std::uint64_t(left)+std::uint64_t(right)+carry_in;
    const auto value=std::uint32_t(sum);
    return {value,((sum >> 32u)&1u)!=0u,((~(left^right)&(left^value)) >> 31u)!=0u};
}
// Keep flag access lazy for packed views, matching the original switch.
template<class Flags>
constexpr bool condition_passed(std::uint8_t condition, const Flags& f) noexcept {
    switch(condition) {
    case 0:return f.z; case 1:return !f.z; case 2:return f.c; case 3:return !f.c;
    case 4:return f.n; case 5:return !f.n; case 6:return f.v; case 7:return !f.v;
    case 8:return f.c&&!f.z; case 9:return !f.c||f.z; case 10:return f.n==f.v;
    case 11:return f.n!=f.v; case 12:return !f.z&&(f.n==f.v); case 13:return f.z||(f.n!=f.v);
    case 14:return true; default:return false;
    }
}
constexpr bool condition_passed(std::uint8_t condition, std::uint32_t cpsr) noexcept {
    struct Flags {bool n,z,c,v;};
    return condition_passed(condition,Flags{(cpsr>>31u)!=0u,((cpsr>>30u)&1u)!=0u,
                             ((cpsr>>29u)&1u)!=0u,((cpsr>>28u)&1u)!=0u});
}
} // namespace arm_recomp::alu
#undef ARM_RECOMP_ALU_INLINE
