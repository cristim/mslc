// EXPECT: error recursive call
// Refs #341: an instance that calls itself is a cycle, which SPIR-V for Vulkan forbids.
#include <metal_stdlib>
using namespace metal;
template <typename T> float f(T x) { return f(x); }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(1); }
