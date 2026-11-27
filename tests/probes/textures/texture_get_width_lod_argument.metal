// EXPECT: valid
// get_width(lod) queries that level.
// DISASM-MATCH: OpImageQuerySizeLod %v2uint %[_0-9a-zA-Z]+ %uint_1(_[0-9])?[^_0-9a-zA-Z]
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  return float4(float(t.get_width(1u)));
}
