// EXPECT: valid
// Apple: texture index 127 is accepted on a texturecube as on a texture2d.
// REFLECT: "metal_index": 127,
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<float> t [[texture(127)]], sampler s [[sampler(0)]]) {
  return t.sample(s, float3(1.0));
}
