// EXPECT: valid
// RUN: grid=16 local=8 cases=2 elems=256
// A typedef of array<T, N>, a braced list assigned to an array, array<T, N>{...} as a value.
#include <metal_stdlib>
using namespace metal;
typedef array<float, 3> Vec3Array;
kernel void array_std_typedef_and_braced_assign(device float* out [[buffer(0)]], device const float* in [[buffer(1)]],
                                                uint i [[thread_position_in_grid]])
{
    Vec3Array a = {in[i], 2.0f, 3.0f};
    Vec3Array b = a;
    a = {in[i + 1], 5.0f, 6.0f};
    b = Vec3Array{b[2], b[1], b[0]};
    Vec3Array c = array<float, 3>{7.0f, 8.0f, in[i + 2]};
    out[i * 6 + 0] = a[0] + a[2];
    out[i * 6 + 1] = b[0];
    out[i * 6 + 2] = b[2];
    out[i * 6 + 3] = c[2] + c[0];
    out[i * 6 + 4] = float(a.size());
    out[i * 6 + 5] = a[1];
}
