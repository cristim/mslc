// EXPECT: valid
// Apple: accepted (black-box, xcrun metal, macOS 26).
#include <metal_stdlib>
using namespace metal;
constexpr sampler gs(filter::linear);
thread const sampler& rs(thread const sampler& x) { return x; }
float4 g(texture2d<float> t, thread const sampler& x) { return t.sample(x, float2(0.5)); }
fragment float4 f(texture2d<float> t [[texture(0)]]) { return t.sample(rs(gs), float2(0.5)) + g(t, gs); }
