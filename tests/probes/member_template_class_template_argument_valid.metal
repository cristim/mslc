// EXPECT: valid
// Refs #341: slice 5d: an instance of another class template built from a member template's parameter, with a braced initialiser.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
struct S { template <typename U> float f(U x) const { Box<U> b{x}; return float(b.v); } };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ S s; out[i] = s.f(2.0f) + float(s.f(3)); }
