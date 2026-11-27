// EXPECT: valid
// DISASM-MATCH: OpConstant %ulong 1[^0-9]
// DISASM-MATCH: OpConstant %ulong 2[^0-9]
// DISASM-MATCH: OpConstant %ulong 4[^0-9]
// DISASM-MATCH: OpConstant %ulong 8[^0-9]
//
// The type of ?: is the arms' type when they agree (a char stays a char, a half a half) and the
// usual arithmetic conversion of the two when they are different scalars: short with int is 4,
// int with ulong is 8. The condition and the arms are never evaluated. Apple: 1, 2, 4 and 8.
#include <metal_stdlib>
using namespace metal;
kernel void sizeof_expression_conditional(device ulong *out [[buffer(0)]])
{
    bool yes = true;
    char c = 0;
    half h = 0;
    int i = 0;
    short s = 0;
    ulong u = 0;
    out[0] = sizeof(yes ? c : c);
    out[1] = sizeof(yes ? h : h);
    out[2] = sizeof(yes ? s : i);
    out[3] = sizeof(yes ? i : u);
}
