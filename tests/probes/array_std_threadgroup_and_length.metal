// EXPECT: valid
// RUN: grid=16 local=8 cases=2
// A threadgroup array<float, 8>, and size() of an array as the length of another.
#include <metal_stdlib>
using namespace metal;
kernel void array_std_threadgroup_and_length(device float* out [[buffer(0)]], device const float* in [[buffer(1)]],
                                             uint i [[thread_position_in_grid]], uint t [[thread_position_in_threadgroup]])
{
    threadgroup array<float, 8> tg;
    array<float, 4> a = {in[i], 1.0f, 2.0f, 3.0f};
    float b[a.size()];
    for (uint j = 0; j < a.size(); ++j) { b[j] = a[j] * 2.0f; }
    tg[t] = b[0] + float(t);
    threadgroup_barrier(mem_flags::mem_threadgroup);
    out[i * 3 + 0] = tg[(t + 1) % 8] + b[3];
    out[i * 3 + 1] = float(tg.size());
    out[i * 3 + 2] = float(sizeof(b) / sizeof(float));
}
