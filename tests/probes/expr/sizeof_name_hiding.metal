// EXPECT: valid
// DISASM: OpTypeArray %float %uint_6
// DISASM: OpTypeArray %float %uint_2
// DISASM: OpTypeArray %float %uint_16
// DISASM: OpTypeArray %float %uint_1
// DISASM: OpTypeArray %float %uint_28
// DISASM: OpTypeArray %float %uint_32
// DISASM: OpTypeArray %float %uint_4
// DISASM: OpTypeArray %float %uint_22
// DISASM: OpTypeArray %float %uint_36
// DISASM: OpTypeArray %float %uint_8
// DISASM-NOT: OpTypeArray %float %uint_24
// DISASM-NOT: OpTypeArray %float %uint_20
// DISASM-NOT: OpTypeArray %float %uint_64
// DISASM-NOT: OpTypeArray %float %uint_48
// DISASM-NOT: OpTypeArray %float %uint_80
// DISASM-NOT: OpTypeArray %float %uint_12
//
// The innermost declaration of a name decides its size, so a variable that hides another, in a
// nested block, a for initialiser, a parameter, a namespace, or by the name of a struct tag,
// sizes as itself: half table[3] (6) over float table[6] (24), ushort (2), a float4 (16) over
// the file-scope float G[5] (20), a uchar parameter (1) over it, G2 used above the float4x4 G2
// that follows it (28, not 64), a float2x4 T (32) over struct T (48), a file-scope uint U (4)
// over struct U (80), a half NG[11] (22) over the namespace's float NG[3] (12), and a half4 G3
// (8) that ::G3 steps over to the file-scope float G3[9] (36). Apple's
// compiler reads the same lengths, and none of the excluded ones.
#include <metal_stdlib>
using namespace metal;
struct T { float4 a[3]; };
struct U { float4 a[5]; };
constant uint U = 1;
constant float G[5] = {1, 2, 3, 4, 5};
constant float G2[7] = {1, 2, 3, 4, 5, 6, 7};
constant float G3[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
namespace n {
    constant float NG[3] = {1, 2, 3};
    float shadowed()
    {
        half NG[11];
        float a[sizeof(NG)];
        return a[0] + float(NG[0]);
    }
}
float byParameter(uchar G)
{
    float a[sizeof(G)];
    return a[0] + float(G);
}
kernel void sizeof_name_hiding(device float *out [[buffer(0)]])
{
    float table[6];
    float sum = 0;
    {
        half table[3];
        float a[sizeof(table)];
        sum += a[0] + float(table[0]);
    }
    for (ushort table = 0; table < 2; ++table) {
        float a[sizeof(table)];
        sum += a[0];
    }
    {
        float4 G;
        float a[sizeof(G)];
        sum += a[0] + G.x;
    }
    {
        float a[sizeof(G2)];
        float4x4 G2;
        sum += a[0] + G2[0][0];
    }
    {
        float2x4 T;
        float a[sizeof(T)];
        sum += a[0] + T[0][0];
    }
    {
        half4 G3;
        float a[sizeof(G3)];
        float b[sizeof(::G3)];
        sum += a[0] + b[0] + float(G3.x);
    }
    float u[sizeof(U)];
    out[0] = sum + u[0] + table[0] + byParameter(1) + n::shadowed();
}
