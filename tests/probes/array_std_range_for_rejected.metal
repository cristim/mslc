// EXPECT: error needs an initialiser
// Apple's array has no begin() and end() either, so a range-for over it is rejected by both.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* o [[buffer(0)]])
{
    array<float, 3> a = {1, 2, 3};
    for (auto x : a) { o[0] += x; }
}
