// EXPECT: valid
#include <metal_stdlib>
using namespace metal;
int first_over(int limit)
{
    for (int n = 0; n < 100; n = n + 1) {
        if (n * n > limit) {
            return n;
        }
    }
    return -1;
}
kernel void k(device int* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = first_over(50); }
