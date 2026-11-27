// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// A "<" in parentheses inside the dependent return type is an operator: the name after it is still found.
#include <metal_stdlib>
using namespace metal;
template <typename T, int N> struct Arr { T v[N]; };
template <typename T> Arr<T, (1 < 2) + 1> make(T x) { Arr<T, 2> o; o.v[0] = x; o.v[1] = x + T(5); return o; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { Arr<float, 2> a = make(float(i)); out[i] = a.v[1]; }
