// EXPECT: error an operator template is not lowered
#include <metal_stdlib>
using namespace metal;
template <typename T> T operator+(T a, T b) { return a; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = 1.0f; }
