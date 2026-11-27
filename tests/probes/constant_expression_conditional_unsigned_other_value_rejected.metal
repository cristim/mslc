// EXPECT: error would wrap as an unsigned one
// The value not chosen is a uint, so the conditional is a uint ([expr.cond]) and -1 converts to UINT_MAX: C++
// takes the 4. The evaluator does not model the conversion, so it refuses.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { float a[(true ? -1 : 0u) > 0 ? 4 : 2]; out[i] = float(sizeof(a)/4); }
