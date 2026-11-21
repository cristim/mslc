// EXPECT: valid
// A compound assignment through a helper's device pointer parameter reads and
// writes the caller's element in place.
#include <metal_stdlib>
using namespace metal;
void scaleAt(device float* p, uint i, float s) { p[i] *= s; }
kernel void helper_device_pointer_compound(device float* out [[buffer(0)]],
                                           uint i [[thread_position_in_grid]])
{
    scaleAt(out, i, 2.0f);
}
