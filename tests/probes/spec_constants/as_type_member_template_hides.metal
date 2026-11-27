// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// DISASM: OpConvertFToU
// DISASM-NOT: OpBitcast %uint
// A member function template named as_type, called unqualified from another member, is the member:
// class scope is searched before metal::as_type. Its value is the conversion T(v), not the bits.
#include <metal_stdlib>
using namespace metal;
struct S {
  template <typename T> T as_type(float v) const { return T(v); }
  uint g(float y) const { return as_type<uint>(y); }
};
kernel void k(device const float* in [[buffer(0)]], device uint* out [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  S s;
  out[i] = s.g(in[i]);
}
