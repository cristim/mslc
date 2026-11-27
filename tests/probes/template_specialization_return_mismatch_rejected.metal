// EXPECT: error the explicit specialization of "f" at line 6 matches no function template of that name declared above it
// The instance f<short> returns a short; a specialization returning a float specializes nothing.
#include <metal_stdlib>
using namespace metal;
template <typename T> T f(T x) { return x; }
template <> float f(short x) { return float(x); }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = float(f(short(i))); }
