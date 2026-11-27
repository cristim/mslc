// EXPECT: valid
// Apple accepts get_width() on a texturecube; a cube's size is two lanes (width, height).
// DISASM: OpCapability ImageQuery
// DISASM-MATCH: OpImageQuerySizeLod %v2uint %[_0-9a-zA-Z]+ %uint_0(_[0-9])?[^_0-9a-zA-Z]
// DISASM-MATCH: OpCompositeExtract %uint %[_0-9a-zA-Z]+ 0[^_0-9a-zA-Z]
// DISASM-NO-MATCH: OpCompositeExtract %uint %[_0-9a-zA-Z]+ 1[^_0-9a-zA-Z]
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return float4(float(t.get_width()));
}
