// EXPECT: error call to "f" passes 2 arguments, and it takes 1
#include <metal_stdlib>
using namespace metal;
int f(int x) { return x + 1; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    for (int j = 0; j < 4; j = f(j, 1)) { out[i] = 1.0f; }
}
