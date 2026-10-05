// EXPECT: valid
// embedded_samplers is always present, empty when the function declares none, so a reader
// never has to test for the key.
// REFLECT: "embedded_samplers": []
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, float2(0.25));
}
