// EXPECT: error the explicit specialization of "f" is declared and not defined here
#include <metal_stdlib>
using namespace metal;
template <typename T> float f(T x) { return float(x); }
template <> float f(short x);
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = f(short(i)); }
