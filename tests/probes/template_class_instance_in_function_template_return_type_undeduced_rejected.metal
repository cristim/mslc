// EXPECT: error no matching function for call to "make"
// T appears only in the return type Box<T>, which does not deduce it, as in C++.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
template <typename T> Box<T> make() { Box<T> b; b.v = T(7); return b; }
kernel void k(device float* out [[buffer(0)]]) { Box<float> b = make(); out[0] = b.v; }
