// EXPECT: error array<T, N> of an entry point
// A buffer of arrays (device array<float, 4>*) is not lowered: the pointee would be read as the element.
#include <metal_stdlib>
using namespace metal;
kernel void k(device array<float, 4>* b [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    b[i][0] = 1.0f;
}
