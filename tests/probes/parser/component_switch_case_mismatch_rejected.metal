// EXPECT: error case labels of type metal::component
#include <metal_stdlib>
kernel void hit(device int* o [[buffer(0)]]) { switch (metal::component::x) { case 0: o[0] = 1; break; } }
