// EXPECT: valid
// DISASM: OpTypeImage %float 1D 2 0 0 1 Unknown
// DISASM: OpImageSampleExplicitLod %v4float
// DISASM: OpFConvert %v4half
// DISASM-MATCH: OpImageFetch %v4float %[0-9a-z_]+ %[0-9a-z_]+ Lod %uint_0
//
// texture1d<half> in a kernel: an explicit lod, the half narrowing after the lookup,
// an int and a half coordinate converted to float, and a literal-zero lod that Apple accepts.
#include <metal_stdlib>
using namespace metal;
constexpr sampler lin(coord::normalized, address::clamp_to_edge, filter::linear);
kernel void k(texture1d<half> t [[texture(0)]], device half4* o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  o[i] = t.sample(lin, half(0.5)) + t.sample(lin, 1) + t.read(i, 0) + half(t.get_width(0));
}
