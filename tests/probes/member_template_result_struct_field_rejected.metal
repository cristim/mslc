// EXPECT: error is not a swizzle
// Refs #341: a struct result that is a template parameter is known only once deduced, so a field read on it is refused (as for a function template).
#include <metal_stdlib>
using namespace metal;
struct P { float v; };
struct S { template <typename U> U id(U x) const { return x; } };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ S s; P p{2.0f}; out[i] = s.id(p).v; }
