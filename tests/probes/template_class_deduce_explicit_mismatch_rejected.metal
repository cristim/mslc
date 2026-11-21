// EXPECT: error no matching function for call to "unbox"
// Refs #341: slice 5e: explicit arguments override deduction: unbox<float> does not take a Box<int>; Apple rejects it.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
template <typename T> T unbox(Box<T> b) { return b.v; }
kernel void k(device float* out [[buffer(0)]]) { Box<int> b; b.v = 4; out[0] = unbox<float>(b); }
