// EXPECT: error a static member function template is not lowered
// Refs #341
#include <metal_stdlib>
using namespace metal;
struct S { float a; template <typename U> static U f(U x) { return x + x; } };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = S::f(2.0f); }
