// EXPECT: error scoped enum metal::component
#include <metal_stdlib>
using namespace metal;
kernel void hit(device int* o [[buffer(0)]]) { int n = component::y; o[0] = n; }
