// EXPECT: error call to "f" is ambiguous
// A uint reaches int and float by a conversion each; neither is better (Apple: ambiguous).
#include <metal_stdlib>
using namespace metal;
float f(int x) { return 1.0f; }
float f(float x) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(1u); }
