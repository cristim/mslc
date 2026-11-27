// EXPECT: error the pointer to bool conversion is not lowered yet
// An array converts to bool, and Apple finds f(bool,int) and f(thread float*,float) ambiguous.
#include <metal_stdlib>
using namespace metal;
float f(bool b, int i) { return 1.0f; }
float f(thread float *p, float x) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]]) { float a[2] = { 1.0f, 2.0f }; out[0] = f(a, 1); }
