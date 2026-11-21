// EXPECT: error no matching function for call to "unbox"
// Refs #341: slice 5e: a device lvalue does not bind thread const Box<T>&; Apple rejects it.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
template <typename T> T unbox(const thread Box<T>& b) { return b.v; }
kernel void k(device Box<float>* p [[buffer(0)]], device float* out [[buffer(1)]]) { out[0] = unbox(p[0]); }
