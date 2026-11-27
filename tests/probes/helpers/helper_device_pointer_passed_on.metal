// EXPECT: valid
// The pointer a helper received is passed on to another helper as the same
// whole-buffer pointer, not re-wrapped.
#include <metal_stdlib>
using namespace metal;
float inner(device float* p, uint i) { return p[i]; }
float outer(device float* p, uint i) { return inner(p, i) * 2.0f; }
kernel void helper_device_pointer_passed_on(device float* out [[buffer(0)]], device float* in [[buffer(1)]],
                                            uint i [[thread_position_in_grid]])
{
    out[i] = outer(in, i);
}
