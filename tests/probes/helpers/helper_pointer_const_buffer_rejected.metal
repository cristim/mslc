// EXPECT: error argument 1 of the call to "f" is const, and parameter "p" is a pointer that can change it
#include <metal_stdlib>
using namespace metal;
void f(device float* p, uint i, float v) { p[i] = v; }
kernel void helper_pointer_const_buffer_rejected(device float* out [[buffer(0)]],
                                                 device const float* in [[buffer(1)]],
                                                 uint i [[thread_position_in_grid]])
{
    f(in, i, 1.0f);
    out[i] = 0.0f;
}
