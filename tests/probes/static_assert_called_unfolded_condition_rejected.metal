// EXPECT: error not an integer constant expression
// A condition mslc cannot fold is refused once the function is lowered.
#include <metal_stdlib>
using namespace metal;
inline float f() { static_assert(1.5f > 1.0f); return 1.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = f(); }
