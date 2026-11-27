// EXPECT: error no matching function for call to "mk"
// Refs #341: slice 5e: a return type alone does not deduce T; Apple rejects it.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
template <typename T> Box<T> mk() { Box<T> b; b.v = T(1); return b; }
kernel void k(device float* out [[buffer(0)]]) { Box<float> b = mk(); out[0] = b.v; }
