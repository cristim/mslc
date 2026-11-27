// EXPECT: error scoped enum metal::component
#include <metal_stdlib>
kernel void hit(device int4* o [[buffer(0)]]) { int4 v(0); v.x = metal::component::x; o[0] = v; }
