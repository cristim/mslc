// EXPECT: valid
// DISASM-MATCH: OpConstant %ulong 4[^0-9]
// DISASM-NO-MATCH: OpConstant %ulong 2[^0-9]
// DISASM-NO-MATCH: OpConstant %ulong 8[^0-9]
//
// A float beside a half or any integer, a long included, is a float (4): half + float, long + float
// and a conditional of short and float. Apple gives 4 for each.
#include <metal_stdlib>
using namespace metal;
kernel void sizeof_expression_float_beats_integer(device ulong *out [[buffer(0)]])
{
    half h = 0;
    long l = 0;
    short s = 0;
    float f = 0;
    out[0] = sizeof(h + f);
    out[1] = sizeof(l + f);
    out[2] = sizeof(true ? s : f);
    out[3] = sizeof(1 + 2.0f);
}
