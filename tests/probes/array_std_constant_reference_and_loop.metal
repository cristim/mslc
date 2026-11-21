// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// A file-scope constant array<float, 3> copied into a local, a reference local to an array<T, N>,
// an empty braced list (zero elements), and a loop over size().
#include <metal_stdlib>
using namespace metal;
constant array<float, 3> kTable = {1.5f, 2.5f, 3.5f};
kernel void array_std_constant_reference_and_loop(device float* out [[buffer(0)]], device const float* in [[buffer(1)]],
                                                  uint i [[thread_position_in_grid]])
{
    array<float, 3> c = kTable;
    c[0] = in[i];
    array<float, 3> a = {in[i + 1], in[i + 2], in[i + 3]};
    thread array<float, 3>& r = a;
    r[1] = 42.0f;
    array<float, 3> z = {};
    float acc = 0.0f;
    for (uint j = 0; j < a.size(); ++j) {
        acc += a[j] * float(j + 1);
    }
    out[i * 6 + 0] = c[0] + kTable[0];
    out[i * 6 + 1] = c[2];
    out[i * 6 + 2] = a[1];
    out[i * 6 + 3] = z[2] + z[0];
    out[i * 6 + 4] = acc;
    out[i * 6 + 5] = kTable[i % 3u];
}
