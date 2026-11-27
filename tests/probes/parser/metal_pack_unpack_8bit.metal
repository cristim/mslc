// EXPECT: valid
// DISASM-MATCH: OpExtInst %v4int %[0-9]+ FindUMsb
// DISASM-MATCH: OpExtInst %v4int %[0-9]+ SAbs
// DISASM: OpShiftRightArithmetic %v4int
// DISASM-MATCH: OpExtInst %v4float %[0-9]+ Pow
// DISASM: OpFConvert %v4half
// The 4x8 unpack functions of <metal_pack> (MSL specification 6.15.1): the first component in the lowest
// byte, unorm as c / 255, snorm as max(-1, c / 127), each correctly rounded, and srgb converting the colour, not the alpha, to linear.
#include <metal_stdlib>
#include <metal_pack>
using namespace metal;
kernel void k(device const uint* in [[buffer(0)]], device float4* out [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  out[i] = unpack_unorm4x8_to_float(in[i]) + unpack_snorm4x8_to_float(in[i]) + unpack_unorm4x8_srgb_to_float(in[i])
    + float4(unpack_unorm4x8_to_half(in[i]) + unpack_snorm4x8_to_half(in[i]) + unpack_unorm4x8_srgb_to_half(in[i]));
}
