// EXPECT: valid
// A texture1d_array size is two wide, width at 0 and the layer count at 1.
// DISASM-MATCH: OpImageQuerySizeLod %v2uint
// DISASM-MATCH: OpCompositeExtract %uint %[0-9A-Za-z_]+ 1[^0-9]
// DISASM-NOT: OpCompositeExtract %uint %[0-9A-Za-z_]+ 2
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture1d_array<float> a [[texture(0)]], device uint* o [[buffer(0)]]) {
  o[0] = a.get_array_size();
}
