// EXPECT: error is not a shape mslc can name
// Refs #341: an out-of-line definition of a member template is refused, so it is never taken for a free function template of that name.
#include <metal_stdlib>
using namespace metal;
struct S { float a; template <typename U> float f(U x) const; };
template <typename U> float S::f(U x) const { return a + float(x); }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ S s{2.0f}; out[i] = s.f(1); }
