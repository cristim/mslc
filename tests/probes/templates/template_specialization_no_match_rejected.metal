// EXPECT: error the explicit specialization of "f" at line 5 matches no function template of that name declared above it
#include <metal_stdlib>
using namespace metal;
template <typename T> float f(T x) { return float(x); }
template <> float f(short x, short y) { return float(x + y); }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = f(short(i)); }
