// EXPECT: valid
// DISASM: OpCapability ImageQuery
// DISASM-MATCH: OpImageQueryLevels %uint
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<float> a [[texture(0)]], texturecube<float> c [[texture(1)]], texture3d<half> d [[texture(2)]], device uint* o [[buffer(0)]]) {
  o[0] = a.get_num_mip_levels() + c.get_num_mip_levels() + d.get_num_mip_levels();
}
