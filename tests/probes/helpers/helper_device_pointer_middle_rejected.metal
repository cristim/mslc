// EXPECT: error argument 1 of the call to "f" has to name a whole buffer, because parameter "p" is a pointer in the device address space, and &b[i] and other pointers into the middle of one are not lowered yet
#include <metal_stdlib>
using namespace metal;
float f(device float* p, uint i) { return p[i]; }
kernel void helper_device_pointer_middle_rejected(device float* out [[buffer(0)]], device float* in [[buffer(1)]],
                                                  uint i [[thread_position_in_grid]])
{
    out[i] = f(&in[i], i);
}
