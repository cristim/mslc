// EXPECT: valid
// A data member named as_type, read unqualified in a member function, is the member: "as_type < 1.0f"
// compares it.
#include <metal_stdlib>
using namespace metal;
struct S {
  float as_type;
  uint f() const { return as_type < 1.0f ? 1u : 2u; }
};
kernel void k(device const float* in [[buffer(0)]], device uint* out [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  S s;
  s.as_type = in[i];
  out[i] = s.f();
}
