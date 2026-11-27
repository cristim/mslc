// EXPECT: error reference to "sqrt" is ambiguous
#include <metal_stdlib>
using metal::precise::sqrt;
using metal::fast::sqrt;
kernel void math_using_precise_and_fast_ambiguous_rejected(device float* out [[buffer(0)]])
{
    out[0] = sqrt(4.0f);
}
