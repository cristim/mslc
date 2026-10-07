// EXPECT: error is a coord::pixel sampler, which a helper function does not take
#include <metal_stdlib>
using namespace metal;
float4 look(texture2d<float> t, sampler s) { return t.sample(s, float2(0.25)); }
fragment float4 f(texture2d<float> t [[texture(0)]]) { constexpr sampler s(coord::pixel); return look(t, s); }
