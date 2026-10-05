// EXPECT: valid
// Two declarations with the same state are one sampler variable and one binding, as
// Apple gives equal states one global (air.sampler_states).
// REFLECT: "embedded_sampler": 0,
// REFLECT-NOT: "embedded_sampler": 1,
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler a(filter::linear);
  constexpr sampler b(mag_filter::linear, min_filter::linear);
  return t.sample(a, float2(0.25)) + t.sample(b, float2(0.25));
}
