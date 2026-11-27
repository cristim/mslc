// EXPECT: error points at a constexpr sampler, which is const
// Apple: refused, "cannot initialize a variable of type metal::sampler * with an rvalue of type const metal::sampler *" (black-box, xcrun metal, macOS 26).
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) { constexpr sampler ls(filter::linear); thread sampler* q = &ls; return t.sample(*q, float2(0.5)); }
