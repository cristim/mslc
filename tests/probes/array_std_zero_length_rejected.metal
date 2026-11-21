// EXPECT: error array<T, 0> is not lowered
// Apple accepts array<float, 0> and gives it the size of one element; mslc refuses it.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* o [[buffer(0)]])
{
    array<float, 0> a;
    o[0] = 1.0f;
}
