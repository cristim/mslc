// EXPECT: valid
// A constant pointer parameter takes a whole buffer in constant memory. A store
// through it is refused, the same way a constant buffer is: Apple's constant
// space is read-only.
#include <metal_stdlib>
using namespace metal;
float readConst(constant float* p, uint i) { return p[i]; }
kernel void helper_constant_pointer_param(device float* out [[buffer(0)]],
                                          constant float* in [[buffer(1)]],
                                          uint i [[thread_position_in_grid]])
{
    out[i] = readConst(in, i);
}
