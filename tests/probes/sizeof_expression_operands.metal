// EXPECT: valid
// DISASM: OpConstant %ulong 1
// DISASM: OpConstant %ulong 2
// DISASM: OpConstant %ulong 12
// DISASM: OpConstant %ulong 24
// DISASM: OpConstant %ulong 32
// DISASM: OpConstant %ulong 48
// DISASM: OpConstant %ulong 64
// DISASM: OpConstant %ulong 16
//
// sizeof of a name, an element or a member is the size of its declared type, read from the
// source: a packed_float3 member is 12 although the value of one is a float3, a buffer's element
// is its struct, and a swizzle is an unpacked vector. The operand is never evaluated. Apple's
// compiler gives 1, 2, 12 twice, 24, 32 three times, 16 (the swizzle), 48 and 64 for these.
#include <metal_stdlib>
using namespace metal;
struct Rec { float3 a; packed_float3 b; };
struct Wide { float4x4 m; };
kernel void sizeof_expression_operands(device ulong *out [[buffer(0)]],
    device Rec *recs [[buffer(1)]], constant Wide &w [[buffer(2)]])
{
    uchar byte = 0;
    half small = 0;
    float table[6];
    Rec local;
    out[0] = sizeof(byte);
    out[1] = sizeof(small);
    out[2] = sizeof(recs[1].b) + sizeof(local.b);
    out[3] = sizeof(table);
    out[4] = sizeof(recs[0]) + sizeof(*recs) + sizeof(local) + sizeof(recs->a.xyz) * 0;
    out[5] = sizeof(w.m[0]) * 0 + sizeof(table[1]) * 0 + sizeof(float3x3);
    out[6] = sizeof(w);
}
