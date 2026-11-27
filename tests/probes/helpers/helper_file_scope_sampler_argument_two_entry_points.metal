// EXPECT: valid
// The same helper serves two entry points when the sampler arrives as an argument,
// and each lists the sampler it reaches.
// REFLECT: "name": "a"
// REFLECT: "name": "b"
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
float4 look(texture2d<float> t, sampler s) { return t.sample(s, float2(0.25)); }
fragment float4 a(texture2d<float> t [[texture(0)]]) { return look(t, S); }
fragment float4 b(texture2d<float> t [[texture(0)]]) { return look(t, S); }
