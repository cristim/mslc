// EXPECT: error scoped enum metal::component
#include <metal_stdlib>
kernel void hit(device int2* o [[buffer(0)]]) { int2 v = metal::component::x; o[0] = v; }
