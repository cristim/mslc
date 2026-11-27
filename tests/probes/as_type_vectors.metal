// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// DISASM: OpBitcast %v4int
// DISASM: OpBitcast %v2half
// DISASM: OpBitcast %uint
// as_type<T>(x) between a vector and a vector or a scalar of the same size in bytes: float4 to int4,
// uchar4 to uint, uint to half2, ushort2 to float.
#include <metal_stdlib>
using namespace metal;
kernel void k(device const float4* in [[buffer(0)]], device uint4* out [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  int4 iv = as_type<int4>(in[i]);
  uint packed = as_type<uint>(uchar4(1, 2, 3, uchar(i)));
  half2 h = as_type<half2>(packed);
  float f = as_type<float>(ushort2(ushort(i), 0x3f80));
  out[i] = uint4(iv) + uint4(packed, as_type<uint>(h), as_type<uint>(f), 0);
}
