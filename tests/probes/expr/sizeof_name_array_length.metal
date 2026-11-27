// EXPECT: valid
// DISASM: OpTypeArray %float %uint_24
// DISASM: OpTypeArray %float %uint_32
// DISASM: OpTypeArray %float %uint_16
// DISASM: OpTypeArray %float %uint_1
// DISASM: OpTypeArray %float %uint_20
// DISASM: OpTypeArray %float %uint_6
// DISASM: OpTypeArray %float %uint_8
// DISASM: OpTypeArray %float %uint_60
// DISASM: OpTypeArray %float %uint_12
// DISASM: OpTypeArray %float %uint_4
// DISASM: OpTypeArray %float %uint_3
// DISASM: OpTypeArray %float %uint_44
// DISASM: OpTypeArray %float %uint_48
// DISASM: OpTypeArray %float %uint_26
//
// sizeof of a declared name is the size of its declared type wherever a constant is read, here
// an array length: a local array, struct, vector and scalar, a file-scope constant array, a
// kernel pointer (8) and a reference parameter (the struct), a two-dimensional array and its
// rows, the element-count idiom, the unparenthesised forms, a threadgroup array and a constexpr
// built from one, and the second declarator of a list. Apple's compiler reads the same lengths: 24, 32, 16, 1, 20, 6, 8, 60, 12,
// 4, 3, 6, 24, 4, 44, 48, 32 and 26, which add to the 1480 bytes it stores.
#include <metal_stdlib>
using namespace metal;
struct S { float4 a; float b; };
constant float G[5] = {1, 2, 3, 4, 5};
constant half H[7] = {1, 2, 3, 4, 5, 6, 7};
kernel void sizeof_name_array_length(device ulong *out [[buffer(0)]], constant S &cs [[buffer(1)]])
{
    float table[6];
    S s;
    float3 v;
    uchar u8 = 0;
    float m[3][5];
    threadgroup uint tile[11];
    thread S &rs = s;
    float a0[sizeof(table)];
    float a1[sizeof(s)];
    float a2[sizeof(v)];
    float a3[sizeof(u8)];
    float a4[sizeof(G)];
    float a5[sizeof(H) / 2 * 2 - 8];
    float a6[sizeof(out)];
    float a7[sizeof(cs) * 0 + sizeof(m) * 1];
    float a8[sizeof(m[0]) - 8];
    float a9[sizeof(m[1][2])];
    float a10[sizeof(m) / sizeof(m[0])];
    float a11[sizeof(table) / sizeof(table[0])];
    float a12[sizeof table];
    float a13[sizeof table[0]];
    float a14[sizeof(tile)];
    constexpr ulong K = sizeof(v) * 3;
    float a15[K];
    float a16[sizeof(rs)];
    half e0, e1[13];
    float a17[sizeof(e1)];
    out[0] = sizeof(a0) + sizeof(a1) + sizeof(a2) + sizeof(a3) + sizeof(a4) + sizeof(a5) + sizeof(a6)
        + sizeof(a7) + sizeof(a8) + sizeof(a9) + sizeof(a10) + sizeof(a11) + sizeof(a12) + sizeof(a13)
        + sizeof(a14) + sizeof(a15) + sizeof(a16) + sizeof(a17);
    out[1] = table[0] + s.b + v.x + u8 + m[0][0] + tile[0] + rs.b + a0[0] + a1[0] + a2[0] + a3[0] + a4[0]
        + a5[0] + a6[0] + a7[0] + a8[0] + a9[0] + a10[0] + a11[0] + a12[0] + a13[0] + a14[0] + a15[0] + a16[0] + a17[0] + float(e0 + e1[0]);
}
