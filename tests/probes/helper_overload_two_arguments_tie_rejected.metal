// EXPECT: error call to "f" is ambiguous
// Each overload is better on one argument and worse on the other (Apple: ambiguous).
#include <metal_stdlib>
using namespace metal;
float f(float x, int y) { return 1.0f; }
float f(int x, float y) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(1, 1); }
