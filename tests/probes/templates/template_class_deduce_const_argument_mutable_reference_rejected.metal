// EXPECT: error no matching function for call to "unbox"
// Refs #341: slice 5e: a const Box<float> does not bind thread Box<T>&; Apple rejects it.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
template <typename T> T unbox(thread Box<T>& b) { return b.v; }
kernel void k(device float* out [[buffer(0)]]) { const Box<float> b = { 4.0f }; out[0] = unbox(b); }
