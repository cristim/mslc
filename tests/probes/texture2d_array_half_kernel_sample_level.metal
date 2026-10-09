// EXPECT: valid
// DISASM: OpImageSampleExplicitLod %v4float
// DISASM: OpFConvert %v4half
// DISASM: OpUConvert %v2uint
//
// A kernel samples a texture2d_array at an explicit lod; a half texture is
// sampled as float and narrowed. read takes a ushort2, a layer and a lod, and
// helpers take the texture like any other.
#include <metal_stdlib>
using namespace metal;
half4 look(texture2d_array<half> t, sampler s, uint layer) { return t.sample(s, float2(0.5), layer, level(1.0)); }
kernel void k(texture2d_array<half> t [[texture(0)]], sampler s [[sampler(0)]],
              device half4 *out [[buffer(0)]], uint id [[thread_position_in_grid]]) {
  out[id] = look(t, s, id) + t.read(ushort2(1, 2), id, 0);
}
