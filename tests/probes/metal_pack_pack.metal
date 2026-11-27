// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// DISASM-MATCH: OpExtInst %v4float %[0-9]+ NClamp
// DISASM-MATCH: OpExtInst %v4float %[0-9]+ RoundEven
// DISASM: OpIsNan %v4bool
// DISASM: OpConvertFToU %v4uint
// DISASM: OpConvertFToS %v2int
// DISASM: OpShiftLeftLogical %v4uint
// DISASM-NOT: PackUnorm4x8
// The pack functions of <metal_pack> (MSL specification 6.15.2), written as the conversions of 8.7.1.2
// (clamp, round to nearest even, NaN to 0) and 8.7.7 (sRGB), not with GLSL.std.450's Pack instructions.
#include <metal_stdlib>
using namespace metal;
kernel void k(device const float4* in [[buffer(0)]], device uint* out [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  const float4 f = in[i];
  out[i] = pack_float_to_unorm4x8(f) ^ pack_float_to_snorm4x8(f) ^ pack_float_to_srgb_unorm4x8(f)
    ^ pack_half_to_unorm4x8(half4(f)) ^ pack_half_to_snorm4x8(half4(f)) ^ pack_half_to_srgb_unorm4x8(half4(f))
    ^ pack_float_to_unorm2x16(f.xy) ^ pack_float_to_snorm2x16(f.zw)
    ^ pack_half_to_unorm2x16(half2(f.xy)) ^ pack_half_to_snorm2x16(half2(f.zw));
}
