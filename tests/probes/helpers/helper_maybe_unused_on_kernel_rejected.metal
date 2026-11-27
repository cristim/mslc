// EXPECT: error [[maybe_unused]] at file or namespace scope is only lowered ahead of a helper function
#include <metal_stdlib>
using namespace metal;

[[maybe_unused]] kernel void k(device float* out [[buffer(0)]]) { out[0] = 1.0f; }
