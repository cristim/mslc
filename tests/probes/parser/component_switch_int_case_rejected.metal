// EXPECT: error scoped enum metal::component
#include <metal_stdlib>
kernel void hit(device int* o [[buffer(0)]]) { switch (0) { case metal::component::x: o[0] = 1; break; } }
