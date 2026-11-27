// EXPECT: valid
// Indium builds the image view from the texture the app binds, not from the reflection, so
// a cube is the same Texture entry a 2D texture is.
// REFLECT: { "kind": "Texture", "metal_index": 0, "descriptor": { "set": 1, "binding": 0 }, "texture_access": "Sample", "param_index": 0, "name": "t" }
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<half> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return float4(t.sample(s, float3(1.0)));
}
