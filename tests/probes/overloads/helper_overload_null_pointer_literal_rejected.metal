// EXPECT: error the null pointer conversion is not lowered yet
// Apple converts the literal 0 to the thread pointer (f(thread float*)), which beats int3.
#include <metal_stdlib>
using namespace metal;
float f(thread float *p) { return 1.0f; }
float f(int3 v) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(0); }
