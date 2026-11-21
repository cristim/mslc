// EXPECT: error an array is used where an array of another length
// array<float, 3> and array<float, 4> are different types and do not assign.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* o [[buffer(0)]])
{
    array<float, 3> a = {1, 2, 3};
    array<float, 4> b;
    b = a;
    o[0] = b[0];
}
