// EXPECT: valid
// DISASM-MATCH: OpConstant %ulong 8[^0-9]
// DISASM-NO-MATCH: OpConstant %ulong 4[^0-9]
// DISASM-NO-MATCH: OpConstant %ulong 1[^0-9]
//
// A long or ulong beside any integer keeps its 64 bits (8), whichever side it is on and whatever
// the signedness, and an int beside a uint is only 4 when neither is 64 bits wide: int + long,
// uint + long, ulong + int, long * char, long << int and a conditional of int and ulong. Apple
// gives 8 for each.
#include <metal_stdlib>
using namespace metal;
kernel void sizeof_expression_keeps_wide_integer(device ulong *out [[buffer(0)]])
{
    int i = 0;
    uint ui = 0;
    long l = 0;
    ulong u = 0;
    char c = 0;
    out[0] = sizeof(i + l);
    out[1] = sizeof(ui + l);
    out[2] = sizeof(u + i);
    out[3] = sizeof(l * c);
    out[4] = sizeof(l << i);
    out[5] = sizeof(true ? i : u);
}
