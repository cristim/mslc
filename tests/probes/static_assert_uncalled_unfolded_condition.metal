// EXPECT: valid
// mslc does not fold a floating-point comparison in a constant expression: in a function that is never lowered
// that is not an error.
#include <metal_stdlib>
using namespace metal;
inline float f() { static_assert(1.5f > 1.0f); return 1.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = 1; }
