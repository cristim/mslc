// EXPECT: valid
// DISASM: OpImageSampleExplicitLod %v4float
// DISASM: OpFConvert %v4half
// DISASM: OpUConvert %v3uint
// DISASM: OpImageQuerySizeLod %v3uint
//
// A kernel has no derivatives, so a texture3d is sampled at an explicit lod,
// and a half texture is sampled as float and narrowed. read takes a ushort3
// and a lod, and get_depth a lod.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture3d<half> t [[texture(0)]], sampler s [[sampler(0)]],
              device half4 *out [[buffer(0)]], uint id [[thread_position_in_grid]]) {
  out[id] = t.sample(s, float3(0.5), level(1.0)) + t.read(ushort3(1, 2, 3), 1) + half(t.get_depth(1));
}
