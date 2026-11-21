// EXPECT: error no matching function for call to "f"
// Refs #341
#include <metal_stdlib>
using namespace metal;
struct S { template <typename U> float f() const { return 1.0f; } };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ S s; out[i] = s.f(); }
