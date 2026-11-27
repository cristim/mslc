// EXPECT: error cannot call the entry point "k"
#include <metal_stdlib>
using namespace metal;
template <typename T> kernel void k(device T* out [[buffer(0)]]) { out[0] = T(1); }
kernel void main_k(device float* out [[buffer(0)]]) { k(out); }
