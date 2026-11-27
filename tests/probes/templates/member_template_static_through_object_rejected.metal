// EXPECT: error is called through an object, which is not lowered
// Refs #341: Apple takes s.f(x) on a static member template; mslc refuses it loudly.
#include <metal_stdlib>
using namespace metal;
struct S { float a; template <typename U> static U f(U x) { return x + x; } };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ S s{1.0f}; out[i] = s.f(2.0f); }
