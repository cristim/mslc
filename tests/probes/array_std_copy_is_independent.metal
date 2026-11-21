// EXPECT: valid
// RUN: grid=64 local=8 cases=3 elems=2048
// DISASM-MATCH: = OpTypeArray %float %uint_3
// array<float, 3> copies by value: changing the copy leaves the original, assigning one array to
// another copies the elements, a row taken out of an array of arrays is a copy of that row.
#include <metal_stdlib>
using namespace metal;
kernel void array_std_copy_is_independent(device float* out [[buffer(0)]], device const float* in [[buffer(1)]],
                                          uint i [[thread_position_in_grid]])
{
    array<float, 3> a = {in[i], in[i + 1], in[i + 2]};
    array<float, 3> b = a;
    b[0] = 100.0f;
    array<float, 3> c;
    c = a;
    c[1] = 200.0f;
    a[2] = 300.0f;
    array<float, 3> d = {{in[i], 5.0f, 6.0f}};
    array<array<float, 2>, 3> m = {{{in[i], 1.0f}, {2.0f, 3.0f}, {4.0f, in[i + 1]}}};
    array<float, 2> row = m[1];
    row[0] = 9.0f;
    m[2] = row;
    row[1] = -1.0f;
    out[i * 16 + 0] = a[0] + 0.0f;
    out[i * 16 + 1] = a[1];
    out[i * 16 + 2] = a[2];
    out[i * 16 + 3] = b[0];
    out[i * 16 + 4] = b[1];
    out[i * 16 + 5] = b[2];
    out[i * 16 + 6] = c[0];
    out[i * 16 + 7] = c[1];
    out[i * 16 + 8] = c[2];
    out[i * 16 + 9] = d[1] + d[0];
    out[i * 16 + 10] = m[1][0];
    out[i * 16 + 11] = m[2][0];
    out[i * 16 + 12] = m[2][1];
    out[i * 16 + 13] = row[1];
    out[i * 16 + 14] = float(a.size() + m.size() + m[0].size());
    out[i * 16 + 15] = a.front() + a.back();
}
