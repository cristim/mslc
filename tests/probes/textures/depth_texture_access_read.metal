// EXPECT: valid
// Depth textures with access::read are sampled depth images (a fetched cube or cube array uses a 2D array of faces).
// DISASM-MATCH: OpTypeImage %float 2D 1 0 0 1 Unknown
// DISASM-MATCH: OpTypeImage %float 2D 1 1 0 1 Unknown
// DISASM: OpImageFetch %v4float
// DISASM-MATCH: OpCompositeExtract %float %[0-9A-Za-z_]+ 0
// DISASM: OpImageQueryLevels %uint
// DISASM: OpImageQuerySizeLod %v3uint
// REFLECT: "texture_access": "Read"
#include <metal_stdlib>
using namespace metal;
kernel void hit(depth2d<float, access::read> a [[texture(0)]],
    depth2d_array<float, access::read> b [[texture(1)]],
    depthcube<float, access::read> c [[texture(2)]],
    depthcube_array<float, access::read> d [[texture(3)]],
    device float4* o [[buffer(0)]]) {
  float r = a.read(uint2(0), 1) + b.read(uint2(1), 2u, 0) + c.read(uint2(0), 3u, 1) + d.read(uint2(1), 2u, 1u, 0);
  uint q = a.get_num_mip_levels() + b.get_array_size() + c.get_num_mip_levels() + c.get_width(0) + d.get_array_size() + d.get_num_mip_levels();
  o[0] = float4(r, float(q), float(a.get_height()), float(d.get_height(1)));
}
