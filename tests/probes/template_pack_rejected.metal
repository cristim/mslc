// EXPECT: error a template parameter pack is not lowered
#include <metal_stdlib>
using namespace metal;
template <typename... Ts> void f(Ts... a) {}
kernel void k(device float* out [[buffer(0)]]) { out[0] = 1.0f; }
