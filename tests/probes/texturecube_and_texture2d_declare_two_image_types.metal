// EXPECT: valid
// One module with both: the 2D image and the cube image are distinct types, the bindings
// follow declaration order and the Metal indices count among textures only.
// DISASM: OpTypeImage %float Cube 2 0 0 1 Unknown
// DISASM: OpTypeImage %float 2D 2 0 0 1 Unknown
// REFLECT: { "kind": "Texture", "metal_index": 0, "descriptor": { "set": 1, "binding": 0 }, "texture_access": "Sample", "param_index": 0, "name": "a" }
// REFLECT: { "kind": "Texture", "metal_index": 1, "descriptor": { "set": 1, "binding": 1 }, "texture_access": "Sample", "param_index": 1, "name": "c" }
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> a [[texture(0)]], texturecube<float> c [[texture(1)]], sampler s [[sampler(0)]]) {
  return a.sample(s, float2(0.5)) + c.sample(s, float3(0.5));
}
