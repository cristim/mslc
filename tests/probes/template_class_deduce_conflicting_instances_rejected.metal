// EXPECT: error no matching function for call to "add"
// Refs #341: slice 5e: two Box<T> parameters given Box<float> and Box<int> deduce T twice; Apple rejects it.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
template <typename T> T add(Box<T> a, Box<T> b) { return a.v + b.v; }
kernel void k(device float* out [[buffer(0)]]) { Box<float> a; a.v = 1.0f; Box<int> b; b.v = 2; out[0] = add(a, b); }
