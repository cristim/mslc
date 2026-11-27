// EXPECT: error is a constexpr sampler, which is const
// Apple: refused, no matching function: a constexpr sampler is const (black-box, xcrun metal, macOS 26).
#include <metal_stdlib>
using namespace metal;
constexpr sampler gs(filter::linear);
thread sampler& rs(thread sampler& x) { return x; }
fragment float4 f(texture2d<float> t [[texture(0)]]) { return t.sample(rs(gs), float2(0.5)); }
