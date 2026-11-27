// EXPECT: valid
// DISASM: OpConstant %ulong 2
// DISASM: OpConstant %ulong 8
// DISASM: OpConstant %ulong 1
// DISASM: OpConstant %ulong 4
//
// sizeof of an arithmetic result is the size of the promoted type: half + half stays a half
// (2), char + char is an int (4), a comparison is a bool (1) and a half3 product is 8. Apple's
// compiler gives the same figures. The operands are typed from the declarations, then the value
// is built and dropped, so nothing in them runs.
#include <metal_stdlib>
using namespace metal;
kernel void sizeof_expression_arithmetic(device ulong *out [[buffer(0)]])
{
    half h = 0;
    char c = 0;
    half3 h3 = 0;
    int i = 0;
    out[0] = sizeof(h + h);
    out[1] = sizeof(c + c) * 100;
    out[2] = sizeof(h3 * h3) + 1000;
    out[3] = sizeof(i < 3);
}
