// EXPECT: valid
// A local of the same name hides the program-scope one from its declaration on; the local's state wins.
// REFLECT: "mag_filter": "Nearest"
// REFLECT-NOT: "mag_filter": "Linear"
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler S(filter::nearest);
  return t.sample(S, float2(0.25));
}
