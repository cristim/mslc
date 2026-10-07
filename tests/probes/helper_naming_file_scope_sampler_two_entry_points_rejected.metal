// EXPECT: error helper function "look" names a file-scope sampler, and entry points "a" and "b" both reach it
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
float4 look(texture2d<float> t) { return t.sample(S, float2(0.25)); }
fragment float4 a(texture2d<float> t [[texture(0)]]) { return look(t); }
fragment float4 b(texture2d<float> t [[texture(0)]]) { return look(t); }
