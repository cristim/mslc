// EXPECT: error "g" is called before it is declared
#include <metal_stdlib>
using namespace metal;
float f(float x) { return g(x); }
float g(float x) { return x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(1.0f); }
