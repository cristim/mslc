// EXPECT: valid
// DISASM: OpCapability Sampled1D
// DISASM: OpTypeImage %float 1D 2 0 0 1 Unknown
// DISASM: OpTypeSampledImage
// DISASM: OpImageSampleImplicitLod %v4float
// DISASM-MATCH: OpImageFetch %v4float %[0-9a-z_]+ %[0-9a-z_]+ Lod
// DISASM: OpImageQuerySizeLod %uint
// DISASM-NOT: OpCompositeExtract %uint
// DISASM: OpCapability ImageQuery
// REFLECT: { "kind": "Texture", "metal_index": 0, "descriptor": { "set": 1, "binding": 0 }, "texture_access": "Sample"
//
// texture1d<float>: sample takes a scalar float coordinate, read a scalar uint, and
// get_width is the whole size, which for a 1D image is a scalar.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture1d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, 0.25f) + t.read(t.get_width() - 1u);
}
