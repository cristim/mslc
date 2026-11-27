// EXPECT: valid
// With no buffer parameter there is no address block, so the first texture is binding
// 0, and a fragment function is in set 1.
// DISASM-MATCH: OpDecorate %[_0-9a-zA-Z]+ DescriptorSet 1
// DISASM-MATCH: OpDecorate %[_0-9a-zA-Z]+ Binding 0
// REFLECT: { "kind": "Texture", "metal_index": 0, "descriptor": { "set": 1, "binding": 0 }, "texture_access": "Sample", "param_index": 0, "name": "t" }
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  return t.read(uint2(0));
}
