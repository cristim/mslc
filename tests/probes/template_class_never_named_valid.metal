// EXPECT: valid
// Refs #341: a class template nobody names is compile-time only and never reaches SPIR-V (as in clang).
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 1.0f; }
