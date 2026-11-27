// EXPECT: valid
#include <metal_stdlib>
using namespace metal;
struct S { float2 a; float b; };
float f(S s) { return s.a.x + s.b; }
kernel void k(device float* out [[buffer(0)]], device const S* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{ out[i] = f(in[i]); }
