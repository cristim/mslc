// EXPECT: error the explicit specialization of "f" at line 7 is declared after a use of the instance it specializes
// Apple refuses an explicit specialization after the instance is used; mslc does not let the primary template stand in.
#include <metal_stdlib>
using namespace metal;
template <typename T> float f(T x) { return float(x); }
static float use(short x) { return f(x); }
template <> float f(short x) { return float(x) * 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = use(short(i)); }
