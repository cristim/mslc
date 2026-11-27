// EXPECT: error scoped enum metal::component
#include <metal_stdlib>
kernel void hit(device int* o [[buffer(0)]]) { if (metal::component::y) { o[0] = 1; } }
