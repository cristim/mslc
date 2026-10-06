// EXPECT: error parameter "t" of helper function "f" is a texture or sampler
#include <metal_stdlib>
using namespace metal;
float4 f(texture2d<float> t) { return float4(0); }
kernel void k(device float4* out [[buffer(0)]], texture2d<float> t [[texture(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(t); }
