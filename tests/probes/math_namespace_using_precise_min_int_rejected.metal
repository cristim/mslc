// EXPECT: error min takes float or half scalars or vectors
#include <metal_stdlib>
namespace A { using metal::precise::min; }
kernel void math_namespace_using_precise_min_int_rejected(device int* out [[buffer(0)]])
{
    out[0] = A::min(1, 2);
}
