// EXPECT: error mslc lowers such an initialiser only with literals, constants and the integer and logical operators
// A call is not a specialization constant operation.
#include <metal_stdlib>
using namespace metal;
constant int x [[function_constant(0)]];
constant float fx [[function_constant(1)]];
constant int y = max(x, 1);
kernel void k(device int* out [[buffer(0)]]) { out[0] = y; }
