// EXPECT: error "f" is explicitly specialized, and no function template of that name is declared above it
#include <metal_stdlib>
using namespace metal;
template <> float f(short x) { return float(x); }
template <typename T> float f(T x) { return float(x); }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = f(short(i)); }
