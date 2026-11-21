// EXPECT: valid
// DISASM-MATCH: OpConstant %ulong 1[^0-9]
// DISASM-MATCH: OpConstant %ulong 2[^0-9]
// DISASM-MATCH: OpConstant %ulong 12[^0-9]
// DISASM-MATCH: OpConstant %ulong 8[^0-9]
// DISASM-NO-MATCH: OpConstant %ulong 4[^0-9]
//
// A local that hides a pointer parameter is sized as itself, not as the pointer: a char p in an
// inner block (1), a short p in a nested one stepped by ++ (2), and a packed_float3 p, which
// stays 12 beside a scalar, while the parameter outside both blocks is the pointer (8). Apple
// gives 1, 2, 12 and 8.
#include <metal_stdlib>
using namespace metal;
kernel void sizeof_expression_pointer_parameter_hidden(device ulong *out [[buffer(0)]],
    device float *p [[buffer(1)]])
{
    {
        char p = 0;
        out[0] = sizeof(true ? p : p);
    }
    for (short p = 0; p < 1; p++) {
        out[1] = sizeof(p++);
    }
    {
        packed_float3 p = packed_float3(0.0f);
        out[2] = sizeof(p * 2.0f);
    }
    out[3] = sizeof(p + 1);
}
