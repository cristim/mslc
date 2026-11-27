// EXPECT: error mslc lowers such a constant only as a bool, an int or a uint
// An OpSpecConstantOp takes no floating-point operation in a shader.
#include <metal_stdlib>
using namespace metal;
constant int x [[function_constant(0)]];
constant float fx [[function_constant(1)]];
constant float y = fx * 2.0f;
kernel void k(device float* out [[buffer(0)]]) { out[0] = y; }
