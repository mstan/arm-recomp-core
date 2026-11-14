// Compare both public storage contracts, including access-time control state.
// General registers intentionally alias canonical storage in the packed view;
// the bus observes only the PC/CPSR publication contract, not stale R0-R14.
#include "interpreter.h"
#include "arm_decode.h"
#include "thumb_decode.h"
#include <array>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <type_traits>
#include <vector>

using namespace armv4t;
static unsigned cases;
static void require(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr,"case %u: %s\n",cases,message); std::exit(1); }
}
template<class State> static ArmCpuState snapshot(const State& s) {
    ArmCpuState p{};
    for(unsigned r=0;r<16;++r)p.R[r]=s.R[r];
    p.cpsr=(uint32_t(s.cpsr.n)<<31)|(uint32_t(s.cpsr.z)<<30)|
        (uint32_t(s.cpsr.c)<<29)|(uint32_t(s.cpsr.v)<<28)|
        (uint32_t(s.cpsr.q)<<27)|(uint32_t(s.cpsr.i)<<7)|
        (uint32_t(s.cpsr.f)<<6)|(uint32_t(s.cpsr.t)<<5)|uint32_t(s.cpsr.mode);
    for(unsigned b=0;b<6;++b){p.banked_sp[b]=s.banked_sp[b];
        p.banked_lr[b]=s.banked_lr[b];p.banked_spsr[b]=s.banked_spsr[b];}
    for(unsigned r=0;r<5;++r){p.r8_12_user[r]=s.r8_12_user[r];p.r8_12_fiq[r]=s.r8_12_fiq[r];}
    return p;
}
static CPUState structured(const ArmCpuState& p) {
    CPUState s{};
    for(unsigned r=0;r<16;++r)s.R[r]=p.R[r];
    s.cpsr.n=p.cpsr>>31;s.cpsr.z=(p.cpsr>>30)&1;s.cpsr.c=(p.cpsr>>29)&1;
    s.cpsr.v=(p.cpsr>>28)&1;s.cpsr.q=(p.cpsr>>27)&1;s.cpsr.i=(p.cpsr>>7)&1;
    s.cpsr.f=(p.cpsr>>6)&1;s.cpsr.t=(p.cpsr>>5)&1;s.cpsr.mode=p.cpsr&31;
    s.thumb=s.cpsr.t;
    for(unsigned b=0;b<6;++b){s.banked_sp[b]=p.banked_sp[b];
        s.banked_lr[b]=p.banked_lr[b];s.banked_spsr[b]=p.banked_spsr[b];}
    for(unsigned r=0;r<5;++r){s.r8_12_user[r]=p.r8_12_user[r];s.r8_12_fiq[r]=p.r8_12_fiq[r];}
    return s;
}
struct TestBus : Bus {
    struct Event {uint32_t kind,addr,value,pc,cpsr;bool operator==(const Event&) const=default;};
    std::array<uint8_t,4096> memory{};
    mutable std::vector<Event> events;
    const ArmCpuState& published;
    explicit TestBus(const ArmCpuState& p):published(p){
        for(unsigned i=0;i<memory.size();++i)memory[i]=uint8_t(i*73+19);
    }
    void note(uint32_t k,uint32_t a,uint32_t v) const {
        events.push_back({k,a,v,published.R[15],published.cpsr});
    }
    uint32_t read(uint32_t a,unsigned n){uint32_t v=0;
        for(unsigned j=0;j<n;++j)v|=uint32_t(memory[(a+j)&4095])<<(8*j);
        note(n,a,v);return v;
    }
    void write(uint32_t a,uint32_t v,unsigned n){note(8+n,a,v);
        for(unsigned j=0;j<n;++j)memory[(a+j)&4095]=uint8_t(v>>(8*j));
    }
    uint8_t read8(uint32_t a)override{return read(a,1);}
    uint16_t read16(uint32_t a)override{return read(a,2);}
    uint32_t read32(uint32_t a)override{return read(a,4);}
    void write8(uint32_t a,uint8_t v)override{write(a,v,1);}
    void write16(uint32_t a,uint16_t v)override{write(a,v,2);}
    void write32(uint32_t a,uint32_t v)override{write(a,v,4);}
    uint32_t access_cycles(uint32_t a,uint8_t n,bool seq)const override{
        note(16+n,a,seq);return 1+((a>>2)&3)+n+seq;
    }
    void coproc_write(uint32_t cp,uint32_t op1,uint32_t crn,uint32_t crm,uint32_t op2,uint32_t v)override{
        note(32+cp,(op1<<24)|(crn<<16)|(crm<<8)|op2,v);
    }
    uint32_t coproc_read(uint32_t cp,uint32_t op1,uint32_t crn,uint32_t crm,uint32_t op2)override{
        note(64+cp,(op1<<24)|(crn<<16)|(crm<<8)|op2,0);return 0xa000001f;
    }
    void coproc_cdp(uint32_t cp,uint32_t op1,uint32_t crn,uint32_t crm,uint32_t op2)override{
        note(96+cp,(op1<<24)|(crn<<16)|(crm<<8)|op2,0);
    }
};
static uint32_t rng=0x961247ab;
static uint32_t next(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static ArmCpuState initial(bool thumb,unsigned mode) {
    ArmCpuState p{};
    for(auto& r:p.R)r=next();
    p.R[15]=0x02000100;
    p.cpsr=(next()&0xffffffc0)|(thumb?32:0)|mode;
    for(unsigned b=0;b<6;++b){p.banked_sp[b]=next();p.banked_lr[b]=next();
        p.banked_spsr[b]=(next()&0xf80000e0)|0x1f;}
    for(unsigned r=0;r<5;++r){p.r8_12_user[r]=next();p.r8_12_fiq[r]=next();}
    return p;
}
static void compare(Instr in,unsigned mode=0x13) {
    ++cases;
    ArmCpuState p=initial(in.thumb,mode);
    CPUState s=structured(p);
    // Legacy sync_out at instruction entry drops unmodeled bits.
    ArmCpuState old_published=snapshot(s);
    PackedCpuStateView view(p);view.publish_control();
    TestBus a(old_published),b(p);
    uint32_t ac=0,bc=0;
    auto ar=Interpreter::step(s,a,in,&ac),br=Interpreter::step(view,b,in,&bc);
    auto expected=snapshot(s);
    view.publish_control();
    require(ar==br,"step result");require(ac==bc,"cycles");
    require(std::memcmp(&expected,&p,sizeof(p))==0,"full active/banked state");
    require(a.events==b.events,"bus order/value/control/timing");
    require(a.memory==b.memory,"memory effects");
    require(s.thumb==bool(view.thumb),"Thumb projection");
}
static void exception_returns(bool packed) {
    // Independent architectural expectations: SPSR.T selects ARM/Thumb;
    // target low bits are ignored for that restored state, unlike BX.
    const uint32_t returns[]={0xe1b0f00e,0xe25ef004,0xe8d08000};
    for(unsigned op=0;op<3;++op)for(unsigned thumb=0;thumb<2;++thumb)for(unsigned low=0;low<4;++low) {
        ++cases;
        const uint32_t target=0x300u|low;
        ArmCpuState actual{};
        for(unsigned r=0;r<16;++r)actual.R[r]=0x11110000u+r;
        actual.R[0]=0x2000;actual.R[13]=0x1000;
        actual.R[14]=target+(op==1?4u:0u);actual.R[15]=0x100;
        actual.cpsr=0xf80000d3u; // SVC/ARM, all flags and I/F initially set.
        actual.banked_sp[Bank_User]=0x500;
        actual.banked_lr[Bank_User]=0x9876;
        actual.banked_spsr[Bank_Supervisor]=0xa800001fu|(thumb?0x20u:0u);
        ArmCpuState expected=actual;
        expected.R[13]=0x500;expected.R[14]=0x9876;
        expected.R[15]=target&~(thumb?1u:3u);
        expected.cpsr=actual.banked_spsr[Bank_Supervisor];
        expected.banked_sp[Bank_Supervisor]=actual.R[13];
        expected.banked_lr[Bank_Supervisor]=actual.R[14];
        TestBus bus(actual);
        bus.write32(0x2000,target);bus.events.clear();
        const auto memory=bus.memory;
        const Instr instruction=ArmDecoder::decode(returns[op],0x100);
        uint32_t cycles=0;Interpreter::Result result;
        if(packed) {
            PackedCpuStateView view(actual);view.publish_control();
            result=Interpreter::step(view,bus,instruction,&cycles);
            require(bool(view.thumb)==bool(thumb),"packed restored Thumb projection");
            view.publish_control();
        } else {
            CPUState state=structured(actual);
            result=Interpreter::step(state,bus,instruction,&cycles);
            require(state.thumb==bool(thumb),"structured restored Thumb projection");
            actual=snapshot(state);
        }
        require(result==Interpreter::Result::Branched,"exception return branches");
        require(actual.R[15]==expected.R[15],"PC alignment follows restored SPSR.T");
        require(std::memcmp(&actual,&expected,sizeof actual)==0,"independent full exception-return state");
        require(cycles!=0,"exception return retains cycle accounting");
        require(bus.memory==memory,"exception return does not write memory");
        unsigned reads=0;
        for(const auto& event:bus.events)if(event.kind==4) {
            ++reads;require(event.addr==0x2000 && event.value==target,"LDM reads original return target");
        }
        require(reads==(op==2?1u:0u),"only LDM reads the return target from memory");
    }
}
static void thumb_conditional_decode() {
    // Compare against architectural opcode families, independently of both
    // storage contracts sharing the same decoder/interpreter implementation.
    constexpr uint32_t pc=0x02000100u;
    for(unsigned imm=0;imm<256;++imm) {
        const auto reserved=ThumbDecoder::decode(uint16_t(0xde00u|imm),pc);
        require(reserved.op==IrOp::Undefined && !reserved.is_branch,"all Thumb DExx encodings are undefined");
        const auto swi=ThumbDecoder::decode(uint16_t(0xdf00u|imm),pc);
        require(swi.op==IrOp::SWI && swi.swi_imm==imm,"Thumb DFxx retains SWI immediate");
        const int32_t displacement=imm<128?int32_t(imm):int32_t(imm)-256;
        for(unsigned cond=0;cond<14;++cond) {
            const auto branch=ThumbDecoder::decode(uint16_t(0xd000u|(cond<<8)|imm),pc);
            require(branch.op==IrOp::B && branch.is_branch && branch.cond==Cond(cond),"valid Thumb conditional branches retain condition");
            require(branch.branch_target==uint32_t(pc+4u+displacement*2),"valid Thumb conditional branches retain target");
        }
    }
    std::printf("PASS 256 reserved Thumb DExx encodings, 256 SWIs and 3584 valid conditional branches\n");
}
int main(int argc,char** argv){
    if(argc==2) {
        const bool packed=std::strcmp(argv[1],"--exception-return-packed")==0;
        require(packed || std::strcmp(argv[1],"--exception-return-structured")==0,"known test selection");
        exception_returns(packed);
        std::printf("PASS %u independent exception-return cases (%s storage)\n",cases,packed?"packed":"structured");
        return 0;
    }
    require(argc==1,"test arguments");
    thumb_conditional_decode();
    static_assert(!std::is_copy_constructible_v<PackedCpuStateView>);
    static_assert(!std::is_move_constructible_v<PackedCpuStateView>);
    ArmCpuState p{};p.cpsr=~0u;PackedCpuStateView v(p);
    require(v.cpsr.value()==0xf80000ff,"reserved CPSR bits");
    v.cpsr.c=2;require(uint32_t(v.cpsr.c)==1,"bool nonzero assignment");
    v.cpsr.n=0;v.cpsr.z=v.cpsr.n;require(!v.cpsr.z,"cross-field value assignment");
    ArmCpuState p2{};PackedCpuStateView v2(p2);v2.cpsr.c=v.cpsr.c;
    v.cpsr.c=0;require(v2.cpsr.c,"same-proxy assignment copies value");
    v.thumb=0;require(!v.cpsr.t,"Thumb alias clear");v.cpsr.t=1;require(v.thumb,"Thumb alias set");
    v.R[0]=42;v.R[15]=100;require(p.R[0]==42&&p.R[15]==0,"canonical register / projected PC");
    v.publish_control();require(p.R[15]==100,"explicit control publication");
    unsigned modes[]={0x10,0x11,0x12,0x13,0x17,0x1b,0x1f};
    for(auto mode:modes)for(unsigned flags=0;flags<32;++flags){
        ArmCpuState a=initial(flags&1,mode);a.cpsr=(a.cpsr&0x07ffffff)|(flags<<27);
        auto s=structured(a);PackedCpuStateView w(a);
        for(unsigned cond=0;cond<16;++cond)
            require(Interpreter::cond_passes(Cond(cond),s.cpsr)==Interpreter::cond_passes(Cond(cond),w.cpsr),"condition codes");
        Interpreter::enter_irq(s,0x12345678);Interpreter::enter_irq(w,0x12345678);w.publish_control();
        auto expect=snapshot(s);require(std::memcmp(&expect,&a,sizeof(a))==0,"IRQ banks");
        require(a.R[14]==0x1234567c&&a.R[15]==0x18&&(a.cpsr&0xff)==0xd2,"IRQ independent expectation");
        s=structured(a);Interpreter::enter_swi(s,0x2000022,true);Interpreter::enter_swi(w,0x2000022,true);w.publish_control();
        expect=snapshot(s);require(std::memcmp(&expect,&a,sizeof(a))==0,"SWI banks");
    }
    // Deliberate opcode families plus decoder-driven broad shape coverage.
    const uint32_t edge[]={0xe8b00000,0xe8a00000,0xe8f08001,0xe8e0e100,
        0xe1b0f00e,0xe25ef004,0xe12ffF10,0xe10f0000,0xe14f0000,
        0xe129f000,0xe169f000,0xe321f011,0xe32ff0ff,0xef000004,
        0xe1000091,0xe1400091,0xe1c0e0d0,0xe1c0f0d0,0xe1c0f0f0,
        0xe1002051,0xe1402051,0xe1202051,0xe1602051,0xee110f10,
        0xee070f90,0xe1b0f00f,0xe590f000,0xe4800004,0xe0b00192,
        0xe0100291,0xe0810392,0xe0e10392,0xe1203281};
    for(auto raw:edge)for(auto mode:modes)compare(ArmDecoder::decode(raw,0x02000100),mode);
    for(unsigned raw=0;raw<65536;++raw)compare(ThumbDecoder::decode(uint16_t(raw),0x02000100),modes[raw%7]);
    for(unsigned n=0;n<50000;++n)compare(ArmDecoder::decode(next(),0x02000100),modes[n%7]);
    exception_returns(false);exception_returns(true);
    std::printf("PASS %u decoded instructions, full state, bus/control/cycle transcripts; proxy and exception contracts\n",cases);
}
