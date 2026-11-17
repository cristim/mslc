// EXPECT: error argument 1 of the call to "f" is in the device address space, but parameter "p" takes a pointer in the constant one, and Metal does not convert between address spaces
#include <metal_stdlib>
using namespace metal;
float f(constant float* p, uint i) { return p[i]; }
kernel void helper_pointer_space_mismatch_rejected(device float* out [[buffer(0)]], device float* in [[buffer(1)]],
                                                   uint i [[thread_position_in_grid]])
{
    out[i] = f(in, i);
}
