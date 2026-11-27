// EXPECT: error invalid values combination in sampler initialization
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(coord::pixel, address::repeat);
fragment float4 f(texture2d<float> t [[texture(0)]]) { return t.sample(S, float2(0.25)); }
