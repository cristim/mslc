// EXPECT: valid
// get_width() is the first lane of an OpImageQuerySizeLod at level 0, which needs the
// ImageQuery capability.
// DISASM: OpCapability ImageQuery
// DISASM-MATCH: OpImageQuerySizeLod %v2uint %[_0-9a-zA-Z]+ %uint_0(_[0-9])?[^_0-9a-zA-Z]
// DISASM-MATCH: OpCompositeExtract %uint %[_0-9a-zA-Z]+ 0[^_0-9a-zA-Z]
// DISASM-NO-MATCH: OpCompositeExtract %uint %[_0-9a-zA-Z]+ 1[^_0-9a-zA-Z]
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  return float4(float(t.get_width()));
}
