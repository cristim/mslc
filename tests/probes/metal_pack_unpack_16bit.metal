// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// DISASM: OpConvertUToF %v2float
// DISASM: OpConvertSToF %v2float
// DISASM: OpFConvert %v2half
// The 2x16 unpack functions of <metal_pack>: the first component in the low 16 bits.
#include <metal_stdlib>
using namespace metal;
kernel void k(device const uint* in [[buffer(0)]], device float2* out [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  out[i] = unpack_unorm2x16_to_float(in[i]) + unpack_snorm2x16_to_float(in[i])
    + float2(unpack_unorm2x16_to_half(in[i]) + unpack_snorm2x16_to_half(in[i]));
}
