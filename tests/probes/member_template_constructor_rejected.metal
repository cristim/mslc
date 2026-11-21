// EXPECT: error a constructor template is not lowered
// Refs #341
#include <metal_stdlib>
using namespace metal;
struct S { float a; template <typename U> S(U x) : a(float(x)) {} };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ S s(2); out[i] = s.a; }
