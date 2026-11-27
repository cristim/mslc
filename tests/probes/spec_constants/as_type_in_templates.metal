// EXPECT: valid
// DISASM: OpBitcast %uint
// DISASM: OpBitcast %ushort
// as_type in a function template, written with and without metal::, and as a statement of its own.
#include <metal_stdlib>
using namespace metal;
template <typename T> static uint bits32(T v) { return as_type<uint>(v); }
template <typename Short> static ushort bits16(Short v) { return metal::as_type<ushort>(v); }
kernel void k(device const float* in [[buffer(0)]], device uint* out [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  as_type<uint>(in[i]);
  out[i] = bits32(in[i]) + bits32(int(i)) + uint(bits16(short(i))) + uint(bits16(half(in[i])));
}
