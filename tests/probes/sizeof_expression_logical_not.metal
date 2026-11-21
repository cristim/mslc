// EXPECT: valid
// DISASM-MATCH: OpConstant %ulong 1[^0-9]
// DISASM-NO-MATCH: OpConstant %ulong 4[^0-9]
// DISASM-NO-MATCH: OpConstant %ulong 2[^0-9]
//
// ! gives a bool (1) whatever it is applied to, a char, a short or a float, and is not promoted
// to an int as - and ~ are. Apple: 1 for each.
#include <metal_stdlib>
using namespace metal;
kernel void sizeof_expression_logical_not(device ulong *out [[buffer(0)]])
{
    char c = 0;
    short s = 0;
    float f = 0;
    out[0] = sizeof(!c);
    out[1] = sizeof(!s);
    out[2] = sizeof(!f);
}
