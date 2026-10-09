// EXPECT: valid
// DISASM: OpTypeImage %float 3D 2 0 0 1 Unknown
// DISASM: OpImageSampleImplicitLod %v4float
// DISASM-MATCH: OpImageFetch %v4float %[0-9a-z_]+ %[0-9a-z_]+ Lod
// DISASM: OpImageQuerySizeLod %v3uint
// DISASM-MATCH: OpCompositeExtract %uint %[0-9a-z_]+ 2
// REFLECT: { "kind": "Texture", "metal_index": 0, "descriptor": { "set": 1, "binding": 0 }, "texture_access": "Sample"
//
// texture3d<float>: sample takes a float3 coordinate, read a uint3 one, and
// get_depth is the third component of the size.
#include <metal_stdlib>
using namespace metal;
struct V { float4 p [[position]]; float3 uvw; };
fragment float4 f(V v [[stage_in]], texture3d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  uint d = t.get_width() + t.get_height() + t.get_depth();
  return t.sample(s, v.uvw) + t.read(uint3(d, 0u, 1u));
}
