// EXPECT: error no matching function for call to "S::S"
// Refs #341: a constructor template whose parameter cannot be deduced fails overload resolution.
#include <metal_stdlib>
using namespace metal;
struct S { float a; template <typename U> S(U x, U y) : a(float(x)) {} };
kernel void k(device float* out [[buffer(0)]])
{ S s(1, 2.0f); out[0] = s.a; }
