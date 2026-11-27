// EXPECT: valid
// A candidate wins when no argument is worse and one is better: h(1.0f, 1) is (float, int)
// exactly, and h(2, 2) is (float, int) too (int to float costs the same in either overload's
// first parameter, and the second is exact), which Apple resolves the same way.
#include <metal_stdlib>
using namespace metal;
float h(float x, float y) { return 1.0f; }
float h(float x, int y) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = h(1.0f, 1) + h(2, 2) + h(1.0f, 2.0f); }
