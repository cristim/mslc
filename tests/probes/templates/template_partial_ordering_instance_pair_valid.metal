// EXPECT: valid
// DISASM: %float_22 = OpConstant %float 22
// DISASM-NOT: %float_11 = OpConstant %float 11
// Refs #341
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
template <typename T, typename U> float h(Box<T> x, U y) { return 11.0f; }
template <typename T> float h(Box<T> x, T y) { return 22.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ Box<float> b; b.v = 3.0f; out[i] = h(b, 5.0f); }
