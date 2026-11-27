// EXPECT: valid
// DISASM-MATCH: OpConstant %ulong 1[^0-9]
// DISASM-MATCH: OpConstant %ulong 2[^0-9]
// DISASM-MATCH: OpConstant %ulong 4[^0-9]
// DISASM-MATCH: OpConstant %ulong 8[^0-9]
// DISASM-MATCH: OpConstant %ulong 16[^0-9]
//
// An operand that is more than a name is typed from the innermost declaration each of its names
// finds. The file-scope float4 g is 16; a uchar g hides it in a block (1), a short g in a for
// initialiser (2) and only inside the loop, so g after it is 16 again; a float parameter g (4)
// hides it in a helper, and a long g (8) hides that parameter in an inner block. Apple: 1, 2,
// 16, 4, 8.
#include <metal_stdlib>
using namespace metal;
constant float4 g = float4(0.0f);
ulong param(float g) { return sizeof(g++); }
ulong param_inner(float g) { { long g = 0; return sizeof(g++); } }
kernel void sizeof_expression_hiding(device ulong *out [[buffer(0)]])
{
    {
        uchar g = 0;
        out[0] = sizeof(g++);
    }
    for (short g = 0; g < 1; g++) {
        out[1] = sizeof(g++);
    }
    out[2] = sizeof(g + g);
    out[3] = param(1.0f);
    out[4] = param_inner(1.0f);
}
