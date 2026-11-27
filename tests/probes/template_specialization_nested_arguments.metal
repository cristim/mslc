// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// DISASM: %float_5
// Explicit arguments whose last one closes with the list in one ">>".
#include <metal_stdlib>
using namespace metal;
template <typename T> struct S { T v; };
template <typename T> float f(T x) { return 1.0f; }
template <> float f<S<int>>(S<int> x) { return 5.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { S<int> s; s.v = int(i); out[i] = f(s); }
