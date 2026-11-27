// EXPECT: valid
// REFLECT: "kind": "Texture"
// A scalar typedef naming float is accepted as the sampled type argument.
#include <metal_stdlib>
using namespace metal;
typedef float Sample;
fragment float4 f(texture2d<Sample> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, float2(0.0, 0.0));
}
