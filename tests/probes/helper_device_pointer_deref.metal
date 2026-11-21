// EXPECT: valid
// *p is p[0] through a helper's device pointer parameter; the helper's write is
// seen through the pointer the caller passed, not a copy.
#include <metal_stdlib>
using namespace metal;
void writeFirst(device float* p, float v) { *p = v; }
float readFirst(device float* p) { return *p; }
kernel void helper_device_pointer_deref(device float* out [[buffer(0)]], device float* in [[buffer(1)]],
                                        uint i [[thread_position_in_grid]])
{
    writeFirst(out, in[i]);
    out[i] = readFirst(in);
}
