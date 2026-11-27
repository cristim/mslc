// EXPECT: valid
// DISASM: %float_2 = OpConstant %float 2
// DISASM-NOT: %float_1 = OpConstant %float 1
// Refs #341
#include <metal_stdlib>
using namespace metal;
template <typename T, typename U> float h(T x, U y) { return 1.0f; }
template <typename T> float h(T x, T y) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ float v = 3.0f; out[i] = h(v, v); }
