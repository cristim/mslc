// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// DISASM: OpBitcast %uint
// DISASM: OpBitcast %ushort
// DISASM: OpBitcast %uchar
// DISASM: OpBitcast %float
// DISASM-NOT: OpConvertFToU
// DISASM-NOT: OpConvertUToF
// as_type<T>(x) between scalars of the same size gives x's bits as a T, never the converted value.
#include <metal_stdlib>
using namespace metal;
kernel void k(device const float* in [[buffer(0)]], device uint* out [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  uint u = as_type<uint>(in[i]);
  ushort s = as_type<ushort>(half(2.0h));
  uchar c = as_type<uchar>(char(-3));
  float f = as_type<float>(u ^ 0x80000000u);
  out[i] = u + uint(s) + uint(c) + uint(f < 0.0f ? 1 : 0);
}
