#include <arm_recomp/alu.hpp>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"ALU line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
using namespace arm_recomp::alu;
struct ObservedFlag {bool value;unsigned* reads;operator bool() const {++*reads;return value;}};
int main() {
    // ARM condition truth table, indexed by the four-bit NZCV word.
    constexpr std::uint16_t truths[]={0xf0f0,0x0f0f,0xcccc,0x3333,0xff00,0x00ff,0xaaaa,0x5555,
        0x0c0c,0xf3f3,0xaa55,0x55aa,0x0a05,0xf5fa,0xffff,0};
    for(unsigned c=0;c<16;++c)for(unsigned f=0;f<16;++f)
        CHECK(condition_passed(std::uint8_t(c),std::uint32_t(f<<28u))==bool((truths[c]>>f)&1u));
    CHECK(!condition_passed(255,0u));
    unsigned reads=0;struct Flags {ObservedFlag n,z,c,v;} flags{{false,&reads},{true,&reads},{false,&reads},{false,&reads}};
    CHECK(!condition_passed(12,flags)&&reads==1);reads=0;CHECK(condition_passed(14,flags)&&reads==0);
    struct ShiftCase {ShiftKind kind;unsigned count;std::uint32_t value,carry;};
    constexpr ShiftCase shifts[]={
        {ShiftKind::Lsl,1,2,1},{ShiftKind::Lsl,31,0x80000000u,0},{ShiftKind::Lsl,32,0,1},{ShiftKind::Lsl,33,0,0},
        {ShiftKind::Lsr,1,0x40000000u,1},{ShiftKind::Lsr,31,1,0},{ShiftKind::Lsr,32,0,1},{ShiftKind::Lsr,33,0,0},
        {ShiftKind::Asr,1,0xc0000000u,1},{ShiftKind::Asr,31,0xffffffffu,0},{ShiftKind::Asr,32,0xffffffffu,1},{ShiftKind::Asr,255,0xffffffffu,1},
        {ShiftKind::Ror,1,0xc0000000u,1},{ShiftKind::Ror,31,3,0},{ShiftKind::Ror,32,0x80000001u,1},{ShiftKind::Ror,33,0xc0000000u,1}};
    for(const auto& c:shifts){auto r=shift(0x80000001u,c.kind,c.count,false,false);CHECK(r.value==c.value&&r.carry==c.carry);}
    for(unsigned c=0;c<2;++c)for(unsigned kind=0;kind<4;++kind){auto r=shift(0x80000001u,ShiftKind(kind),0,false,c!=0);CHECK(r.value==0x80000001u&&r.carry==c);}
    CHECK(shift(0x80000001u,ShiftKind::Lsr,0,true,false).value==0);
    CHECK(shift(0x80000001u,ShiftKind::Asr,0,true,false).value==0xffffffffu);
    CHECK(shift(0x80000001u,ShiftKind::Rrx,0,true,false).value==0x40000000u);
    CHECK(shift(0x80000001u,ShiftKind::Rrx,0,true,true).value==0xc0000000u);
    CHECK(rotate_right(0x80000001u,32)==0x80000001u);
    CHECK(arithmetic_shift_right(0x80000001u,32)==0xffffffffu);
    CHECK(arithmetic_shift_right(0x7fffffffu,33)==0);
    CHECK(sign_extend(1,1)==-1&&sign_extend(0x1ff,9)==-1&&sign_extend(0x100,9)==-256);
    CHECK(sign_extend(0xabcdef80u,8)==-128&&sign_extend(0x80000000u,32)==std::numeric_limits<std::int32_t>::min());
    auto a=add_with_carry(0xffffffffu,1,0);CHECK(a.value==0&&a.carry&&!a.overflow);
    a=add_with_carry(0x7fffffffu,0,1);CHECK(a.value==0x80000000u&&!a.carry&&a.overflow);
    a=add_with_carry(0x80000000u,0xffffffffu,0);CHECK(a.value==0x7fffffffu&&a.carry&&a.overflow);
    a=add_with_carry(0x80000000u,0x80000000u,0);CHECK(a.value==0&&a.carry&&a.overflow);
    std::puts("ARM ALU contracts PASS: condition truth table/lazy flags, shift boundaries, RRX, sign extension, carry/overflow");
}
