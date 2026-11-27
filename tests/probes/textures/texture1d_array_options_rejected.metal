// EXPECT: error a texture1d_array samples with a sampler and a coordinate only
// Apple: no matching member function for sample with level on a texture1d_array.
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture1d_array<float> a [[texture(0)]], sampler s [[sampler(0)]]) {
  return a.sample(s, 0.5, 1u, level(1.0));
}
