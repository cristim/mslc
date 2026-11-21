// EXPECT: error cannot store through "p", which is in the constant address space or declared const
#include <metal_stdlib>
using namespace metal;
void writeConst(constant float* p, uint i, float v) { p[i] = v; }
kernel void helper_constant_pointer_write_rejected(device float* out [[buffer(0)]],
                                                   constant float* in [[buffer(1)]],
                                                   uint i [[thread_position_in_grid]])
{
    writeConst(in, i, 1.0f);
    out[i] = 0.0f;
}
