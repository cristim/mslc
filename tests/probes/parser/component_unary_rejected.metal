// EXPECT: error scoped enum metal::component
#include <metal_stdlib>
kernel void hit(device int* o [[buffer(0)]]) { o[0] = int(+metal::component::y); }
