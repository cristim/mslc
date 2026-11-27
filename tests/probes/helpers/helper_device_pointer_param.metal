// EXPECT: valid
// A device pointer parameter takes a whole buffer: the helper indexes it with the
// argument's element, reads and writes through the same pointer the caller gave.
#include <metal_stdlib>
using namespace metal;
float readAt(device float* p, uint i) { return p[i]; }
void bumpAt(device float* p, uint i, float d) { p[i] = p[i] + d; }
kernel void helper_device_pointer_param(device float* out [[buffer(0)]], device float* in [[buffer(1)]],
                                        uint i [[thread_position_in_grid]])
{
    bumpAt(out, i, 2.0f);
    out[i] = readAt(in, i) + readAt(out, i);
}
