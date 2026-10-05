// EXPECT: valid
// A sampler declared inside a block is a global like any other: the entry point has to
// list its variable in the interface, which spirv-val checks.
// REFLECT: "embedded_sampler": 0,
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  float4 r = float4(0.0);
  if (true) {
    constexpr sampler z(filter::linear);
    r = t.sample(z, float2(0.25));
  }
  return r;
}
