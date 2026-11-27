// EXPECT: error mixes sizeof with a negative value or subtracts below zero
// A sizeof in either value of a conditional makes it unsigned, so subtracting below zero wraps.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* out [[buffer(0)]]) { float a[(true ? sizeof(int) : 1) - 5]; out[0] = 1; }
