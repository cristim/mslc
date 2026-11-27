// EXPECT: error cannot mix scoped enum metal::component
#include <metal_stdlib>
kernel void hit(device int* o [[buffer(0)]], uint u [[thread_position_in_grid]]) { o[0] = u > 0 ? metal::component::x : 1; }
