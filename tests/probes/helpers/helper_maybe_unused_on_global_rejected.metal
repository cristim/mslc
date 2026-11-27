// EXPECT: error [[maybe_unused]] at file or namespace scope is only lowered ahead of a helper function
#include <metal_stdlib>
using namespace metal;

[[maybe_unused]] constant int kVal = 1;
kernel void k(device int* out [[buffer(0)]]) { out[0] = kVal; }
