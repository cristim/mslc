// EXPECT: valid
// DISASM-MATCH: OpConstant %ulong 1[^0-9]
// DISASM-MATCH: OpConstant %ulong 2[^0-9]
// DISASM-MATCH: OpConstant %ulong 4[^0-9]
// DISASM-MATCH: OpConstant %ulong 8[^0-9]
// DISASM-MATCH: OpConstant %ulong 12[^0-9]
// DISASM-MATCH: OpConstant %ulong 16[^0-9]
// DISASM-MATCH: OpConstant %ulong 48[^0-9]
// DISASM-NOT: OpIAdd
// DISASM-NOT: OpISub
// DISASM-NOT: OpFunctionCall
// DISASM-NOT: OpFunction %mat3v3float
//
// The operand of sizeof is never evaluated: c++ is a char (1), s += l a short (2), --i an int (4),
// l = 1 a long (8), a packed vector's own assignment 12, a float3 assignment 16, a call a
// float3x3 (48), and none of them leaves an add, a subtract, a call or the called function in the
// module, so i is still 0 where it is read. Apple compiles the same figures (with a warning
// that the operand has no effect).
#include <metal_stdlib>
using namespace metal;
struct Rec { float3 a; packed_float3 b; };
float3x3 widen(int x) { return float3x3(1.0f) * float(x); }
kernel void sizeof_expression_unevaluated(device ulong *out [[buffer(0)]],
    device Rec *recs [[buffer(1)]])
{
    int i = 0;
    char c = 0;
    short s = 0;
    long l = 0;
    float3 v = 0;
    out[0] = sizeof(c++);
    out[1] = sizeof(s += l);
    out[2] = sizeof(--i);
    out[3] = sizeof(l = 1);
    out[4] = sizeof(recs[0].b = recs[0].b);
    out[5] = sizeof(v = v);
    out[6] = sizeof(widen(i++));
    out[7] = i;
}
