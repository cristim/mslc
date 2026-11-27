// EXPECT: valid
// Apple: accepted (black-box, xcrun metal, macOS 26).
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) { constexpr sampler ls(filter::linear); thread const sampler* q = &ls; return t.sample(*q, float2(0.5)); }
