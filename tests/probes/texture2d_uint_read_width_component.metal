// EXPECT: valid
// get_width() on an access::read texture is lane 0 of OpImageQuerySize and nothing else.
// DISASM-MATCH: OpCompositeExtract %uint %[_0-9a-zA-Z]+ 0[^_0-9a-zA-Z]
// DISASM-NO-MATCH: OpCompositeExtract %uint %[_0-9a-zA-Z]+ 1[^_0-9a-zA-Z]
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<uint, access::read> t [[texture(0)]], device uint* o [[buffer(0)]]) {
  o[0] = t.get_width();
}
