// EXPECT: error specializes an instance that is already explicitly specialized
#include <metal_stdlib>
using namespace metal;
template <typename T> float f(T x) { return float(x); }
template <> float f(short x) { return float(x) * 2.0f; }
template <> float f<short>(short x) { return float(x) * 3.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = f(short(i)); }
