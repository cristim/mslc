// EXPECT: valid
// On a texture3d, get_width, get_height and get_depth read lanes 0, 1 and 2 of one
// OpImageQuerySizeLod %v3uint each, in call order.
// DISASM-MATCH: OpCompositeExtract %uint %[_0-9a-zA-Z]+ 0[^_0-9a-zA-Z]
// DISASM-MATCH: OpCompositeExtract %uint %[_0-9a-zA-Z]+ 1[^_0-9a-zA-Z]
// DISASM-MATCH: OpCompositeExtract %uint %[_0-9a-zA-Z]+ 2[^_0-9a-zA-Z]
#include <metal_stdlib>
using namespace metal;
kernel void k(texture3d<float> t [[texture(0)]], device uint* o [[buffer(0)]]) {
  o[0] = t.get_width();
  o[1] = t.get_height();
  o[2] = t.get_depth();
}
