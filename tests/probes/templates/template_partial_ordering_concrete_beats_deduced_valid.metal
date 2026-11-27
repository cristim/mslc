// EXPECT: valid
// DISASM: %float_2 = OpConstant %float 2
// DISASM: %float_4 = OpConstant %float 4
// DISASM-NOT: %float_1 = OpConstant %float 1
// Refs #341
#include <metal_stdlib>
using namespace metal;
template <typename T> float h(T x, T y) { return 2.0f; }
template <typename T, typename U> float h(T x, U y) { return 1.0f; }
template <typename T> float h(T x, int y) { return 4.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ float v = 3.0f; int n = 1; out[i] = h(v, v) + h(v, n); }
