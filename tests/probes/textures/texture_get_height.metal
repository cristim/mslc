// EXPECT: valid
// get_height() is the second lane of the same query. With only this call in the module,
// a swap of the two lanes shows as the wrong index.
// DISASM-MATCH: OpImageQuerySizeLod %v2uint %[_0-9a-zA-Z]+ %uint_0(_[0-9])?[^_0-9a-zA-Z]
// DISASM-MATCH: OpCompositeExtract %uint %[_0-9a-zA-Z]+ 1[^_0-9a-zA-Z]
// DISASM-NO-MATCH: OpCompositeExtract %uint %[_0-9a-zA-Z]+ 0[^_0-9a-zA-Z]
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  return float4(float(t.get_height()));
}
