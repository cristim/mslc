// EXPECT: error is not the type of the place
#include <metal_stdlib>
kernel void hit(device int* o [[buffer(0)]]) { metal::component c = metal::component::x; thread int& r = c; o[0] = r; }
