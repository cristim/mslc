// EXPECT: valid
// A texture1d_array is a Dim 1D arrayed image: sample takes a float and a layer (float2 coordinate), read a uint and a layer, get_array_size is component 1 of a two-wide size.
// DISASM: OpCapability Sampled1D
// DISASM: OpCapability Image1D
// DISASM-MATCH: OpTypeImage %float 1D 2 1 0 1 Unknown
// DISASM-MATCH: OpTypeImage %float 1D 0 1 0 2 Unknown
// DISASM-MATCH: OpImageQuerySizeLod %v2uint
// DISASM-MATCH: OpImageQuerySize %v2uint
// DISASM-MATCH: OpImageFetch %v4float
// DISASM-MATCH: OpImageRead %v4float
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture1d_array<float> a [[texture(0)]], texture1d_array<float, access::read_write> b [[texture(1)]], sampler s [[sampler(0)]], device float4* o [[buffer(0)]], uint t [[thread_position_in_grid]]) {
  o[0] = a.sample(s, 0.5, 1u) + a.read(t, 2u, 0) + b.read(t, 1u);
  b.write(float4(1), t, 2u);
  o[1] = float4(a.get_width(), a.get_array_size(), b.get_width(), b.get_array_size());
}
