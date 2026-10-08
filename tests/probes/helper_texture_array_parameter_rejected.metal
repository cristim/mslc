// EXPECT: error is an array
#include <metal_stdlib>
using namespace metal;
float4 look(texture2d<float> t[2], sampler s) { return float4(0); }
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) { return float4(0); }
