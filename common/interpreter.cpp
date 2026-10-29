// One semantic implementation serves both public storage contracts.
#include "interpreter_impl.h"

namespace armv4t {
uint32_t Interpreter::read_reg(const CPUState& cpu,uint8_t r,const Instr& i) {
    return interpreter_detail::read_reg(cpu,r,i);
}
uint32_t Interpreter::read_reg(const PackedCpuStateView& cpu,uint8_t r,const Instr& i) {
    return interpreter_detail::read_reg(cpu,r,i);
}
bool Interpreter::cond_passes(Cond c,const CPSR& cpsr) {
    return interpreter_detail::cond_passes(c,cpsr);
}
bool Interpreter::cond_passes(Cond c,const PackedCpsrView& cpsr) {
    return interpreter_detail::cond_passes(c,cpsr);
}
void Interpreter::enter_irq(CPUState& cpu,uint32_t address) {
    interpreter_detail::enter_irq(cpu,address);
}
void Interpreter::enter_irq(PackedCpuStateView& cpu,uint32_t address) {
    interpreter_detail::enter_irq(cpu,address);
}
void Interpreter::enter_swi(CPUState& cpu,uint32_t address,bool thumb) {
    interpreter_detail::enter_swi(cpu,address,thumb);
}
void Interpreter::enter_swi(PackedCpuStateView& cpu,uint32_t address,bool thumb) {
    interpreter_detail::enter_swi(cpu,address,thumb);
}
Interpreter::Result Interpreter::step(CPUState& cpu,Bus& bus,const Instr& i,uint32_t* cycles) {
    return interpreter_detail::step(cpu,bus,i,cycles);
}
Interpreter::Result Interpreter::step(PackedCpuStateView& cpu,Bus& bus,const Instr& i,uint32_t* cycles) {
    return interpreter_detail::step(cpu,bus,i,cycles);
}
} // namespace armv4t
