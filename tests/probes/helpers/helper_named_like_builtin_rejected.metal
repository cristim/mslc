// EXPECT: error helper function "max" has the name of a builtin
// Apple: "call to 'max' is ambiguous" once a call matches both.
#include <metal_stdlib>
using namespace metal;
float max(float a, float b) { return a - b; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = max(3.0f, 1.0f); }
