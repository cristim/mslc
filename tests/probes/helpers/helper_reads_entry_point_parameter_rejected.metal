// EXPECT: error "scale" is not a parameter, local, constant or builtin
#include <metal_stdlib>
using namespace metal;
float f(float x) { return x * scale; }
kernel void k(device float* out [[buffer(0)]], constant float& scale [[buffer(1)]], uint i [[thread_position_in_grid]])
{ out[i] = f(1.0f); }
