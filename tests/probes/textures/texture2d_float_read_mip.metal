// EXPECT: valid
// DISASM-MATCH: OpImageFetch %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ Lod %uint_1
//
// read(coord, lod) on an access::read texture reads the level given: the texture is a sampled image of all its
// levels, fetched from, as a Vulkan storage image view has one level only.
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<float, access::read> src, device float4* out [[buffer(0)]]) { out[0] = src.read(uint2(0), 1u); }
