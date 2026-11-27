// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// DISASM: OpUConvert %uint
// DISASM: OpConvertFToU %uint
// DISASM: OpCompositeConstruct %v4float
// A scalar argument is converted as a call converts it (MSL 2.24): an unpack function takes a ushort or a
// float as a uint, and a pack function takes a scalar as the vector of it in every component.
#include <metal_stdlib>
using namespace metal;
kernel void k(device const float* in [[buffer(0)]], device float4* out [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  out[i] = unpack_unorm4x8_to_float(ushort(i)) + unpack_snorm4x8_to_float(in[i] * 1000.0f)
    + float4(float(pack_float_to_unorm4x8(in[i])), float(pack_half_to_snorm2x16(half(in[i]))), 0.0f, 0.0f);
}
