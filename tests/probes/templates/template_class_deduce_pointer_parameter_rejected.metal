// EXPECT: error pointer to an instance of a class template
// Refs #341: slice 5e: a device Box<T>* parameter is not lowered (Apple accepts it).
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
template <typename T> T unbox(device Box<T>* b) { return b->v; }
kernel void k(device Box<float>* p [[buffer(0)]], device float* out [[buffer(1)]]) { out[0] = unbox(p); }
