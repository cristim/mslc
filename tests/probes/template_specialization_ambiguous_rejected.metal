// EXPECT: error matches more than one function template of that name, and choosing among them by partial ordering is not lowered
#include <metal_stdlib>
using namespace metal;
template <typename T> float f(T x) { return float(x); }
template <typename T, typename U = int> float f(T x) { return float(x) + 1.0f; }
template <> float f(short x) { return float(x) * 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = 1.0f; }
