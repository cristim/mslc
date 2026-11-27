// EXPECT: error no matching function for call to "f"
// Refs #341: f(U, U) given a float and an int has no U.
#include <metal_stdlib>
using namespace metal;
struct S { float a; template <typename U> float f(U x, U y) const { return a + float(x) + float(y); } };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ S s{1.0f}; out[i] = s.f(1, 2.0f); }
