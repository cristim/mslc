// EXPECT: error "f" is declared again with a different return type
// Apple: "functions that differ only in their return type cannot be overloaded".
#include <metal_stdlib>
using namespace metal;
float f(float x);
int f(float x) { return 1; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 1.0f; }
