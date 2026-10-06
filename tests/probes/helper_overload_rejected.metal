// EXPECT: error overloading "f" is not lowered
// Apple accepts the overload; the declarations differ in their parameters.
#include <metal_stdlib>
using namespace metal;
float f(float x) { return x; }
float f(float3 x) { return x.x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(1.0f); }
