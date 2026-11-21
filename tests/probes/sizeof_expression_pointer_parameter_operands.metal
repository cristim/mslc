// EXPECT: valid
// DISASM-MATCH: OpConstant %ulong 8[^0-9]
// DISASM-NO-MATCH: OpConstant %ulong 4[^0-9]
// DISASM-NO-MATCH: OpConstant %ulong 16[^0-9]
//
// A pointer parameter in an operand is a pointer (8), not what it points at: p + 1, p++, p = p, a
// conditional of two of them and (0, p) are 8, and the elements it points at are not (4 and 16
// are what p[0] and *q give). Apple: 8 for each of the pointers.
#include <metal_stdlib>
using namespace metal;
kernel void sizeof_expression_pointer_parameter_operands(device ulong *out [[buffer(0)]],
    device float *p [[buffer(1)]], device float4 *q [[buffer(2)]])
{
    out[0] = sizeof(p + 1);
    out[1] = sizeof(p++);
    out[2] = sizeof(p = p);
    out[3] = sizeof(true ? p : p);
    out[4] = sizeof((0, p));
    out[5] = sizeof(1 + q);
}
