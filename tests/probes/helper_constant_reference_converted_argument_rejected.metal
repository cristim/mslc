// EXPECT: error is not the type of parameter
// An int constant does not bind a float reference (it would be a temporary in another address space).
#include <metal_stdlib>
using namespace metal;
constant int KI = 3;
float plus(const constant float &c) { return c + 1.0f; }
kernel void k(device float *o [[buffer(0)]]) { o[0] = plus(KI); }
