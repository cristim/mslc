// EXPECT: valid
// REFLECT: "name": "A" }
// REFLECT: "name": "B" }
// REFLECT: "embedded_sampler": 1,
#include <metal_stdlib>
using namespace metal;
constexpr sampler A {filter::linear};
constexpr sampler B = sampler(filter::nearest, address::repeat);
constexpr sampler C;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  return t.sample(A, float2(0.25)) + t.sample(B, float2(0.25));
}
