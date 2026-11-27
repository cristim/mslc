// EXPECT: valid
// DISASM-MATCH: OpConstant %ulong 2[^0-9]
// DISASM-NO-MATCH: OpConstant %ulong 4[^0-9]
// DISASM-NO-MATCH: OpConstant %ulong 8[^0-9]
//
// A half is not promoted, and beats every integer: half + half, +h, -h, half + short and half
// + ulong are 2 (Apple: 2 for each).
#include <metal_stdlib>
using namespace metal;
kernel void sizeof_expression_half_stays_half(device ulong *out [[buffer(0)]])
{
    half h = 0;
    short s = 0;
    ulong u = 0;
    out[0] = sizeof(h + h);
    out[1] = sizeof(+h);
    out[2] = sizeof(-h);
    out[3] = sizeof(h + s);
    out[4] = sizeof(u + h);
}
