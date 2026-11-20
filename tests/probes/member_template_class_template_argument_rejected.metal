// EXPECT: error is first named inside a function template
// Refs #341: an instance of another class template that depends on a member template's parameter is slice 5d.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
struct S { template <typename U> float f(U x) const { Box<U> b{x}; return float(b.v); } };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ S s; out[i] = s.f(2.0f); }
