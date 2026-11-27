// EXPECT: error no matching function for call to "unbox"
// Refs #341: slice 5e: a float is no Box<T>: deduction does not convert; Apple rejects it.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
template <typename T> T unbox(Box<T> b) { return b.v; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = unbox(1.0f); }
