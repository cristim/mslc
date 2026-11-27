// EXPECT: valid
// DISASM: OpTypeImage %float 2D 2 1 0 1 Unknown
// DISASM: OpImageSampleImplicitLod %v4float
// DISASM-MATCH: OpCompositeConstruct %v3float %[0-9a-z_]+ %[0-9a-z_]+
// DISASM-MATCH: OpCompositeConstruct %v3uint %[0-9a-z_]+ %uint_1
// DISASM-MATCH: OpImageFetch %v4float %[0-9a-z_]+ %[0-9a-z_]+ Lod
// DISASM: OpImageQuerySizeLod %v3uint
// DISASM-MATCH: OpCompositeExtract %uint %[0-9a-z_]+ 2
// REFLECT: { "kind": "Texture", "metal_index": 0, "descriptor": { "set": 1, "binding": 0 }, "texture_access": "Sample"
//
// texture2d_array<float>: sample takes a float2 coordinate and a uint layer,
// read a uint2 coordinate and a layer, both lowered as one arrayed coordinate
// with the layer last; get_array_size is the third component of the size.
#include <metal_stdlib>
using namespace metal;
struct V { float4 p [[position]]; float2 uv; };
fragment float4 f(V v [[stage_in]], texture2d_array<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  uint n = t.get_width() + t.get_height() + t.get_array_size();
  return t.sample(s, v.uv, n - 1u) + t.read(uint2(n, 0u), 1u);
}
