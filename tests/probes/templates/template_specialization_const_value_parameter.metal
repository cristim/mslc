// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// DISASM: %float_2
// A by-value parameter's top-level const is not part of the function type: f(const int) specializes f<int>.
#include <metal_stdlib>
using namespace metal;
template <typename T> float f(T x) { return 1.0f; }
template <> float f(const int x) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = f(int(i)); }
