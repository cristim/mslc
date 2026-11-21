// EXPECT: error deduced from argument
// A function template parameter spelled array<T, 3> is class-template deduction (#341), not lowered here.
#include <metal_stdlib>
using namespace metal;
template <typename T> float first(array<T, 3> a) { return a[0]; }
kernel void k(device float* o [[buffer(0)]])
{
    array<float, 3> a = {1, 2, 3};
    o[0] = first(a);
}
