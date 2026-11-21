// EXPECT: error is an element or member that a mutable reference could bind
// Apple considers the reference viable for v.x (and then errors); mslc refuses.
#include <metal_stdlib>
using namespace metal;
float f(thread float &x) { return 1.0f; }
float f(float x) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]]) { float2 v = float2(1.0f); out[0] = f(v.x); }
