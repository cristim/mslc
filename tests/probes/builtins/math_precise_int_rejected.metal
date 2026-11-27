// EXPECT: error min takes float or half scalars or vectors
#include <metal_stdlib>
using namespace metal;
kernel void math_precise_int_rejected(device int* out [[buffer(0)]])
{
    out[0] = precise::min(1, 2);
}
