// EXPECT: error is not a shape mslc can name
// Only the second-group check refuses this: "__decltype" is not a guarded keyword and
// an attribute follows its group, so the first group passes as a parameter list.
#include <metal_stdlib>
using namespace metal;
template <typename T> __decltype(T()) __attribute__((always_inline)) conv(T c) { return c * 2.0f; }
float conv(float3 c) { return c.x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = conv(1.0f); }
