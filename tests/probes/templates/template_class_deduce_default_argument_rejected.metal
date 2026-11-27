// EXPECT: error leaves an argument to its default
// Refs #341: slice 5e: D<T> with a default second argument while T is still to be deduced is not lowered (Apple accepts it).
#include <metal_stdlib>
using namespace metal;
template <typename T, typename U = T> struct D { T v; U u; };
template <typename T> T dv(D<T> d) { return d.v; }
kernel void k(device float* out [[buffer(0)]]) { D<float> d; d.v = 4.0f; d.u = 1.0f; out[0] = dv(d); }
