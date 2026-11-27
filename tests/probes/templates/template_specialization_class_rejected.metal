// EXPECT: error an explicit specialization of a class template is not lowered
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
template <> struct Box<int> { int v; int w; };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = 1.0f; }
