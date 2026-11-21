// EXPECT: error is not a shape mslc can name
#include <metal_stdlib>
using namespace metal;
template <typename T> T conv(T c) const { return c; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 1.0f; }
