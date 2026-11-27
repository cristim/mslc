// EXPECT: error scoped enum metal::component
#include <metal_stdlib>
kernel void hit(device int* o [[buffer(0)]]) { int4 v(1); o[0] = v[metal::component::y]; }
