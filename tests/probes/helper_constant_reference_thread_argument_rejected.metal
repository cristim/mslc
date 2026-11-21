// EXPECT: error not in the constant address space
// Apple: a thread local does not bind a reference to constant memory.
#include <metal_stdlib>
using namespace metal;
float plus(const constant float &c) { return c + 1.0f; }
kernel void k(device float *o [[buffer(0)]]) { float t = 1.0f; o[0] = plus(t); }
