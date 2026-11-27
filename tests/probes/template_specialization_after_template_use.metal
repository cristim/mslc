// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// DISASM: %float_2
// A use in a template declared before the specialization instantiates where the kernel calls that template,
// which is after the specialization, as clang places it: g(1) reaches the specialization f<int>, and the
// primary's own body may call it.
#include <metal_stdlib>
using namespace metal;
template <typename T> float f(T x) { return 1.0f + f(int(x)); }
template <typename T> float g(T x) { return f(x); }
template <> float f(int x) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = g(1) + 10.0f * g(1.0f); }
