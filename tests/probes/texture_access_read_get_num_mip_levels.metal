// EXPECT: valid
// access::read textures are sampled images in SPIR-V and lower get_num_mip_levels() via OpImageQueryLevels.
// DISASM: OpCapability ImageQuery
// DISASM: OpImageQueryLevels %uint
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture1d<float, access::read> a [[texture(0)]],
    texture1d_array<float, access::read> b [[texture(1)]],
    texture2d<float, access::read> c [[texture(2)]],
    texture2d_array<uint, access::read> d [[texture(3)]],
    texture3d<half, access::read> e [[texture(4)]],
    texturecube<float, access::read> f [[texture(5)]],
    texturecube_array<float, access::read> g [[texture(6)]],
    device uint* o [[buffer(0)]]) {
  o[0] = a.get_num_mip_levels() + b.get_num_mip_levels() + c.get_num_mip_levels()
    + d.get_num_mip_levels() + e.get_num_mip_levels() + f.get_num_mip_levels() + g.get_num_mip_levels();
}
