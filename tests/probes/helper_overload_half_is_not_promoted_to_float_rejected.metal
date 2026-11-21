// EXPECT: error call to "f" is ambiguous
// half to float is a conversion in Apple, not a promotion: f(int)/f(float) with a half is
// ambiguous.
#include <metal_stdlib>
using namespace metal;
float f(int x) { return 1.0f; }
float f(float x) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(1.0h); }
