// EXPECT: valid
// DISASM: OpTypeArray %float %uint_2
// DISASM: OpTypeArray %float %uint_8
// DISASM: OpTypeArray %float %uint_12
// DISASM: OpTypeArray %float %uint_20
// DISASM: OpTypeArray %float %uint_32
// DISASM: OpTypeArray %float %uint_36
//
// A helper's parameters are sized as declared: an array parameter decays to a pointer (8), and
// a row of a decayed float m[3][3] is still an array of 3 (12), not one float; a reference is
// its struct (32) and a uchar2 pointed to is 2. Apple's compiler reads the same lengths.
#include <metal_stdlib>
using namespace metal;
struct S { float4 a; float b; };
float helper(float p[3], thread S &r, thread uchar2 *q, float m[3][3], constant S &cr)
{
    float a0[sizeof(*q)];
    float a1[sizeof(p)];
    float a2[sizeof(m[0])];
    float a3[sizeof(m[1][1]) * 5];
    float a4[sizeof(r)];
    float a5[sizeof(cr) + sizeof(m) / 2];
    return a0[0] + a1[0] + a2[0] + a3[0] + a4[0] + a5[0] + p[0] + r.b + float(q[0].x) + m[0][0] + cr.b;
}
kernel void sizeof_name_helper_parameters(device float *out [[buffer(0)]], constant S &cs [[buffer(1)]])
{
    float p[3] = {1, 2, 3};
    float m[3][3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    uchar2 q[2] = {uchar2(1, 2), uchar2(3, 4)};
    S s;
    s.a = float4(1);
    s.b = 2;
    out[0] = helper(p, s, q, m, cs);
}
